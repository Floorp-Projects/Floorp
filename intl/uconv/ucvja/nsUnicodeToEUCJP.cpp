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

#include "nsUnicodeToEUCJP.h"
#include "nsUCVJADll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// Shift Table
static const PRInt16 g0201ShiftTable[] =  {
        2, uMultibytesCharset,
        ShiftCell(u1ByteChar,           1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
static const PRInt16 g0208ShiftTable[] =  {
        0, u2BytesGRCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g0212ShiftTable[] =  {
        0, u2BytesGRPrefix8FCharset, 
        ShiftCell(0,0,0,0,0,0,0,0)
};
#define SIZE_OF_TABLES 5
static const PRInt16 *gShiftTables[SIZE_OF_TABLES] =  {
    g0208ShiftTable,
    g0208ShiftTable,
    g0201ShiftTable,
    g0201ShiftTable,
    g0212ShiftTable
};

static const PRUint16 *gMappingTables[SIZE_OF_TABLES] = {
    g_uf0208Mapping,
    g_uf0208extMapping,
    g_uf0201Mapping,
    g_uf0201Mapping,
    g_uf0212Mapping
};

//----------------------------------------------------------------------
// Class nsUnicodeToEUCJP [implementation]

nsUnicodeToEUCJP::nsUnicodeToEUCJP() 
: nsMultiTableEncoderSupport(SIZE_OF_TABLES,
                         (uShiftTable**) gShiftTables, 
                         (uMappingTable**) gMappingTables)
{
}

nsresult nsUnicodeToEUCJP::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToEUCJP();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToEUCJP::GetMaxLength(const PRUnichar * aSrc, 
                                             PRInt32 aSrcLength,
                                             PRInt32 * aDestLength)
{
  *aDestLength = 3*aSrcLength;
  return NS_OK;
}
