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


#include "LTextFile.h"

LTextFile::LTextFile( const FSSpec& inFileSpec ):
	LFileStream( inFileSpec )
{
}

LTextFile::LTextFile(): LFileStream()
{
}

ExceptionCode LTextFile::ReadLine( unsigned char* outBuffer, Int32& inOutByteCount,
	unsigned char inDelimiter )
{
	ParamBlockRec			pb;
	ExceptionCode			err;
	Int32					inCount;
	
	// ¥Êset up the pb
	inCount = inOutByteCount;
	pb.ioParam.ioRefNum	= this->GetDataForkRefNum();
	pb.ioParam.ioBuffer	= (Ptr)outBuffer;
	pb.ioParam.ioReqCount = inCount;
	
	// ¥Êcr in high byte, bit 7 = newLine mode
	pb.ioParam.ioPosMode = ( inDelimiter << 8 ) | (0x80 | fsAtMark);
	pb.ioParam.ioPosOffset = 0;
	err = PBReadSync( &pb );
	if ( err != eofErr && err != noErr )
		return err;

	if ( err == eofErr && pb.ioParam.ioActCount == 0 )
		return eofErr;
		
	inOutByteCount = pb.ioParam.ioActCount;
	
	if ( inOutByteCount == inCount )
		return err_DelimiterNotRead;
	if ( outBuffer[ inOutByteCount - 1 ] != inDelimiter )
		return err_NoEOL;
	
	return noErr;
}
