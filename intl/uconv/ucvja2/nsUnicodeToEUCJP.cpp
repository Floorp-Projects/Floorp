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

#include "nsUnicodeToEUCJP.h"
#include "nsUCVJA2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// Shift Table
static PRInt16 g0201ShiftTable[] =  {
        2, uMultibytesCharset,
        ShiftCell(u1ByteChar,           1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
static PRInt16 g0208ShiftTable[] =  {
        0, u2BytesGRCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g0212ShiftTable[] =  {
        0, u2BytesGRPrefix8EA2Charset,
        ShiftCell(0,0,0,0,0,0,0,0)
};
static PRInt16 *gShiftTables[4] =  {
    g0208ShiftTable,
    g0201ShiftTable,
    g0201ShiftTable,
    g0212ShiftTable
};

static PRUint16 *gMappingTables[4] = {
    g_uf0208Mapping,
    g_uf0201Mapping,
    g_uf0201Mapping,
    g_uf0212Mapping
};

//----------------------------------------------------------------------
// Class nsUnicodeToEUCJP [implementation]

nsUnicodeToEUCJP::nsUnicodeToEUCJP() 
: nsTablesEncoderSupport(4,
                         (uShiftTable**) gShiftTables, 
                         (uMappingTable**) gMappingTables)
{
}

nsresult nsUnicodeToEUCJP::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUnicodeToEUCJP();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
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
