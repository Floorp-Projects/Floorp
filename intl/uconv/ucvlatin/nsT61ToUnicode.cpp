/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsT61ToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_T61MappingTable[] = {
#include "t61.ut"
};


static const PRInt16 g_T61ShiftTable[] =  {
    3, uMultibytesCharset,  
    ShiftCell(u1ByteChar,   1, 0x00, 0xBF, 0x00, 0x00, 0x00, 0xBF),
    ShiftCell(u1ByteChar,   1, 0xD0, 0xFF, 0x00, 0xD0, 0x00, 0xFF),
    ShiftCell(u2BytesChar,  2, 0xC0, 0xCF, 0xC0, 0x41, 0xCF, 0x7A)
};

//----------------------------------------------------------------------
// Class nsT61ToUnicode [implementation]

nsT61ToUnicode::nsT61ToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_T61ShiftTable, 
                        (uMappingTable*) &g_T61MappingTable)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsT61ToUnicode::GetMaxLength(const char * aSrc, 
                                            PRInt32 aSrcLength, 
                                            PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}
