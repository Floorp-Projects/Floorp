/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
