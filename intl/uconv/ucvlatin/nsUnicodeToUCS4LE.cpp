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

#include "nsUnicodeToUCS4LE.h"
#include <string.h>

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_UCS4LEMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0xFFFF, 0x0000
};

static const PRInt16 g_UCS4LEShiftTable[] =  {
  0, u4BytesSwapCharset, 
  ShiftCell(0,      0, 0, 0, 0, 0, 0, 0) 
};

//----------------------------------------------------------------------
// Class nsUnicodeToUCS4LE [implementation]

nsUnicodeToUCS4LE::nsUnicodeToUCS4LE() 
: nsTableEncoderSupport((uShiftTable*) &g_UCS4LEShiftTable, 
                        (uMappingTable*) &g_UCS4LEMappingTable)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToUCS4LE::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 4*aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}
NS_IMETHODIMP nsUnicodeToUCS4LE::FillInfo(PRUint32 *aInfo)
{
  memset(aInfo, 0xFF, (0x10000L >> 3));
  return NS_OK;
}
