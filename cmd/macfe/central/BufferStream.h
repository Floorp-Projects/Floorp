/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __BUFFERSTREAM__
#define __BUFFERSTREAM__

#include <LFileStream.h>
#include <stddef.h>

// Does plain buffered read/writes
// Buffering strategy:
//	reading:	read all you can fit in the buffer
//				give it to the client in small chunks
//	writing:	write all you can fit in the buffer
//				on overflow, flush everything, then write the rest
// for now, we only buffer writing
class LFileBufferStream: public LFileStream
{
public:
					LFileBufferStream( FSSpec& inFileSpec );
	virtual			~LFileBufferStream();
	virtual Int32	WriteData( const void *inFromBuffer, Int32 inByteCount );
	virtual Int32	ReadData( void* outToBuffer, Int32 inByteCount );
	virtual	void	CloseDataFork();
	void			DoUseBuffer();
	void			SetURL( char* url ) { fURL = url; }
	char*			GetURL()	{ return fURL; }
protected:
	OSErr			FlushBuffer( Boolean allocateNew );
	Boolean			fUseBuffer;
	Handle			fBuffer;
	UInt32			fBufferSize;
	UInt32			fLastWritten;
	Boolean			fWriteFailed;
	char *			fURL;
};

#endif // __BUFFERSTREAM__

