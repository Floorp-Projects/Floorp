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

#include "BufferStream.h"

#include "client.h"

#ifdef PROFILE
#pragma profile on
#endif

#define STREAM_BUFFER_SIZE	32000

LFileBufferStream::LFileBufferStream( FSSpec& inFileSpec ): LFileStream( inFileSpec )
{
	fBuffer = NULL;
	fBufferSize = 0;
	fLastWritten = 0;
	fUseBuffer = FALSE;
	fURL = NULL;
	fWriteFailed = FALSE;
}

LFileBufferStream::~LFileBufferStream()
{
	Try_
	{
		FlushBuffer( FALSE );
	}
	Catch_(inErr)
	{
	}
	EndCatch_
	if ( fURL )
		XP_FREE( fURL );
}

OSErr LFileBufferStream::FlushBuffer( Boolean allocateNew )
{
	OSErr	err = noErr;
	
	if ( !fUseBuffer )
		return err;

	if ( fBuffer && ( fLastWritten > 0 ) )
	{
		HLock( fBuffer );
		Try_	
		{
			err = LFileStream::PutBytes( *fBuffer, fLastWritten );
			ThrowIfOSErr_(err);
			HUnlock( fBuffer );
		}
		Catch_(inErr)
		{
			HUnlock( fBuffer );
			DisposeHandle(fBuffer);
			fBuffer = NULL;
			fWriteFailed = TRUE;
		}
		EndCatch_
		fLastWritten = 0;
	}
	
	if (fWriteFailed)
		return err;

	if ( allocateNew && ( !fBuffer ) )
	{
		fBuffer = ::NewHandle( STREAM_BUFFER_SIZE );
		fBufferSize = STREAM_BUFFER_SIZE;
		fLastWritten = 0;
	}

	if ( !allocateNew && fBuffer )
	{
		DisposeHandle( fBuffer );
		fBuffer = NULL;
	}
	
	return err;
}

Int32 LFileBufferStream::ReadData( void* outBuffer, Int32 inByteCount )
{
	return LFileStream::ReadData( outBuffer, inByteCount );
}

void LFileBufferStream::DoUseBuffer()
{
	fUseBuffer = TRUE;
}

void LFileBufferStream::CloseDataFork()
{
	FlushBuffer( FALSE );
	LFileStream::CloseDataFork();
}

Int32 LFileBufferStream::WriteData( const void* inFromBuffer, Int32 inByteCount )
{
	OSErr err = noErr;
	
	if ( fUseBuffer && ( fLastWritten + inByteCount ) > fBufferSize )
		err = FlushBuffer( TRUE );
		
	ThrowIfOSErr_(err);

	if ( ( fBuffer ) && 	// If we have space, fill up the buffer
		( ( fLastWritten + inByteCount ) <= fBufferSize ) )
	{
		::BlockMoveData( inFromBuffer, &( (*fBuffer)[fLastWritten] ), inByteCount );
		fLastWritten += inByteCount;
		return inByteCount;
	}
	// Otherwise, just do a normal write
	else
	{
		err = LFileStream::PutBytes( inFromBuffer, inByteCount );
		ThrowIfOSErr_(err);
	}

	return inByteCount;
}


#ifdef PROFILE
#pragma profile off
#endif
