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

#pragma once

#include <LFileStream.h>


const		Int16		refNum_Undefined = -1;
enum {	err_NotFound = 'notF',
		err_DelimiterNotRead = 'notD',
		err_NoEOL = 'notE',
		err_NoValue = 'notV' };

class LTextFile: public LFileStream
{
public:
							LTextFile( const FSSpec& inFileSpec );
							LTextFile();
	virtual ExceptionCode	ReadLine( unsigned char* outBuffer, Int32& inOutByteCount,
								unsigned char inDelimiter = 0x0D /* CR */);
};
