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

#include "nsUnicodeToSJIS.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_SJISMappingTable[] = {
#include "sjis.uf"
};

static const PRInt16 g_SJISShiftTable[] =  {
  4, uMultibytesCharset,
  ShiftCell(u1ByteChar,   1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
  ShiftCell(u1ByteChar,   1, 0xA1, 0xDF, 0x00, 0xA1, 0x00, 0xDF),
  ShiftCell(u2BytesChar,  2, 0x81, 0x9F, 0x81, 0x40, 0x9F, 0xFC),
  ShiftCell(u2BytesChar,  2, 0xE0, 0xFC, 0xE0, 0x40, 0xFC, 0xFC)
};

//----------------------------------------------------------------------
// Class nsUnicodeToSJIS [implementation]

nsUnicodeToSJIS::nsUnicodeToSJIS() 
: nsTableEncoderSupport((uShiftTable*) &g_SJISShiftTable, 
                        (uMappingTable*) &g_SJISMappingTable)
{
}

nsresult nsUnicodeToSJIS::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToSJIS();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToSJIS::GetMaxLength(const PRUnichar * aSrc, 
                                            PRInt32 aSrcLength,
                                            PRInt32 * aDestLength)
{
  *aDestLength = 2*aSrcLength;
  return NS_OK;
}
