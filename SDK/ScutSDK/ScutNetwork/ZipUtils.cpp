/****************************************************************************
Copyright (c) 2013-2015 Scutgame.com

http://www.scutgame.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include <zlib.h>
#include <assert.h>
#include <stdlib.h>
#include "Trace.h"
#include "ZipUtils.h"
//#include "ccMacros.h"

namespace ScutDataLogic
{
	// memory in iPhone is precious
	// Should buffer factor be 1.5 instead of 2 ?
    #define BUFFER_INC_FACTOR (2)

	int ZipUtils::inflateMemory_(unsigned char *in, unsigned int inLength, unsigned char **out, unsigned int *outLength)
	{
		/* ret value */
		int err = Z_OK;

		/* 256k initial decompress buffer */		
		int bufferSize = 256 * 1024;
		if (inLength * 8 > 256 * 1024)
		{
			bufferSize = 8 * inLength;
		}
		*out = new unsigned char[bufferSize];

		z_stream d_stream; /* decompression stream */	
		d_stream.zalloc = (alloc_func)0;
		d_stream.zfree = (free_func)0;
		d_stream.opaque = (voidpf)0;

		d_stream.next_in  = in;
		d_stream.avail_in = inLength;
		d_stream.next_out = *out;
		d_stream.avail_out = bufferSize;

		/* window size to hold 256k */
		if( (err = inflateInit2(&d_stream, 15 + 32)) != Z_OK )
		{
			return err;
		}

		for (;;) {
			err = inflate(&d_stream, Z_NO_FLUSH);

			if (err == Z_STREAM_END)
			{
				break;
			}

			switch (err) {
			case Z_NEED_DICT:
				err = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				inflateEnd(&d_stream);
				return err;
			}

			// not enough memory ?
			if (err != Z_STREAM_END) 
			{
				delete [] *out;
				*out = new unsigned char[bufferSize * BUFFER_INC_FACTOR];

				/* not enough memory, ouch */
				if (! *out ) 
				{
					ScutLog("ScutDataLogic: ZipUtils: realloc failed");
					inflateEnd(&d_stream);
					return Z_MEM_ERROR;
				}

				d_stream.next_out = *out + bufferSize;
				d_stream.avail_out = bufferSize;
				bufferSize *= BUFFER_INC_FACTOR;
			}
		}


		*outLength = bufferSize - d_stream.avail_out;
		err = inflateEnd(&d_stream);
		return err;
	}

	int ZipUtils::ccInflateMemory(unsigned char *in, unsigned int inLength, unsigned char **out)
	{
		unsigned int outLength = 0;
		int err = inflateMemory_(in, inLength, out, &outLength);

		if (err != Z_OK || *out == NULL) {
			if (err == Z_MEM_ERROR)
			{
				ScutLog("ScutDataLogic: ZipUtils: Out of memory while decompressing map data!");
			} else 
			if (err == Z_VERSION_ERROR)
			{
				ScutLog("ScutDataLogic: ZipUtils: Incompatible zlib version!\r\n");
			} else 
			if (err == Z_DATA_ERROR)
			{
				ScutLog("ScutDataLogic: ZipUtils: Incorrect zlib compressed data!\r\n");
			}
			else
			{
				ScutLog("ScutDataLogic: ZipUtils: Unknown error while decompressing map data!\r\n");
			}

			delete[] *out;
			*out = NULL;
			outLength = 0;
		}

		return outLength;
	}

	int ZipUtils::ccInflateGZipFile(const char *path, unsigned char **out)
	{
		int len;
		unsigned int offset = 0;

		assert( out );
		assert( &*out );

		gzFile inFile = gzopen(path, "rb");
		if( inFile == NULL ) {
			ScutLog("ScutDataLogic: ZipUtils: error open gzip file: %s", path);
			return -1;
		}

		/* 512k initial decompress buffer */
		unsigned int bufferSize = 512 * 1024;
		unsigned int totalBufferSize = bufferSize;

		*out = (unsigned char*)malloc( bufferSize );
		if( ! out ) 
		{
			ScutLog("ScutDataLogic: ZipUtils: out of memory");
			return -1;
		}

		for (;;) {
			len = gzread(inFile, *out + offset, bufferSize);
			if (len < 0) 
			{
				ScutLog("ScutDataLogic: ZipUtils: error in gzread");
				free( *out );
				*out = NULL;
				return -1;
			}
			if (len == 0)
			{
				break;
			}

			offset += len;

			// finish reading the file
			if( (unsigned int)len < bufferSize )
			{
				break;
			}

			bufferSize *= BUFFER_INC_FACTOR;
			totalBufferSize += bufferSize;
			unsigned char *tmp = (unsigned char*)realloc(*out, totalBufferSize );

			if( ! tmp ) 
			{
				ScutLog("ScutDataLogic: ZipUtils: out of memory");
				free( *out );
				*out = NULL;
				return -1;
			}

			*out = tmp;
		}

		if (gzclose(inFile) != Z_OK)
		{
			ScutLog("ScutDataLogic: ZipUtils: gzclose failed");
		}

		return offset;
	}

	int ZipUtils::ccInflateCCZFile(const char *path, unsigned char **out)
	{
		///@todo implement CFSwapInt16BigToHost CFSwapInt32BigToHost
		return -1;

// 		assert( out );
// 		assert( &*out );
// 
// 		// load file into memory
// 		unsigned char *compressed = NULL;
// 		int fileLen  = ccLoadFileIntoMemory( path, &compressed );
// 		if( fileLen < 0 ) 
// 		{
// 			CCLOG("ScutDataLogic: Error loading CCZ compressed file");
// 		}
// 
// 		struct CCZHeader *header = (struct CCZHeader*) compressed;
// 
// 		// verify header
// 		if( header->sig[0] != 'C' || header->sig[1] != 'C' || header->sig[2] != 'Z' || header->sig[3] != '!' ) 
// 		{
// 			CCLOG("ScutDataLogic: Invalid CCZ file");
// 			free(compressed);
// 			return -1;
// 		}
// 
// 		// verify header version
// 		
// 		unsigned int version = CFSwapInt16BigToHost( header->version );
// 		if( version > 2 ) 
// 		{
// 			CCLOG("ScutDataLogic: Unsupported CCZ header format");
// 			free(compressed);
// 			return -1;
// 		}
// 
// 		// verify compression format
// 		if( CFSwapInt16BigToHost(header->compression_type) != CCZ_COMPRESSION_ZLIB ) 
// 		{
// 			CCLOG("ScutDataLogic: CCZ Unsupported compression method");
// 			free(compressed);
// 			return -1;
// 		}
// 
// 		uint32_t len = CFSwapInt32BigToHost( header->len );
// 
// 		*out = malloc( len );
// 		if(! *out )
// 		{
// 			CCLOG("ScutDataLogic: CCZ: Failed to allocate memory for texture");
// 			free(compressed);
// 			return -1;
// 		}
// 
// 
// 		uLongf destlen = len;
// 		uLongf source = (uLongf) compressed + sizeof(*header);
// 		int ret = uncompress(*out, &destlen, (Bytef*)source, fileLen - sizeof(*header) );
// 
// 		free( compressed );
// 
// 		if( ret != Z_OK )
// 		{
// 			CCLOG("ScutDataLogic: CCZ: Failed to uncompress data");
// 			free( *out );
// 			*out = NULL;
// 			return -1;
// 		}
// 
// 
// 		return len;
	}

} // end of namespace ScutDataLogic
