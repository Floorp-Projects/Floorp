/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsUTF8ToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_UTF8MappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0xFFFF, 0x0000
};

static PRInt16 g_UTF8ShiftTable[] =  {
  3, uMultibytesCharset, 
  ShiftCell(u1ByteChar,       1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F), 
  ShiftCell(u2BytesUTF8,      2, 0xC0, 0xDF, 0x00, 0x00, 0x07, 0xFF), 
  ShiftCell(u3BytesUTF8,      3, 0xE0, 0xEF, 0x08, 0x00, 0xFF, 0xFF) 
};

//----------------------------------------------------------------------
// Class nsUTF8ToUnicode [implementation]

nsUTF8ToUnicode::nsUTF8ToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_UTF8ShiftTable, 
                        (uMappingTable*) &g_UTF8MappingTable)
{
}

nsresult nsUTF8ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUTF8ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsUTF8ToUnicode::Length(const char * aSrc, 
                                      PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
