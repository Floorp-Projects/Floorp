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

#include "nsEUCJPToUnicode.h"
#include "nsUCVJA2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// Shift Table
static PRInt16 g_0201ShiftTable[] =  {
        2, uMultibytesCharset,
        ShiftCell(u1ByteChar,           1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
static PRInt16 g_0208ShiftTable[] =  {
        0, u2BytesGRCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g_0212ShiftTable[] =  {
        0, u2BytesGRPrefix8FCharset, 
        ShiftCell(0,0,0,0,0,0,0,0)
};
static PRInt16 *g_EUCJPShiftTable[4] =  {
    g_0208ShiftTable,
    g_0201ShiftTable,
    g_0201ShiftTable,
    g_0212ShiftTable
};

static PRUint16 *g_EUCJPMappingTable[4] = {
    g_ut0208Mapping,
    g_ut0201Mapping,
    g_ut0201Mapping,
    g_ut0212Mapping
};

static uRange g_EUCJPRanges[] = {
    { 0xA1, 0xFE },
    { 0x00, 0x7F },
    { 0x8E, 0x8E },
    { 0x8F, 0x8F } 
};

//----------------------------------------------------------------------
// Class nsEUCJPToUnicode [implementation]

nsEUCJPToUnicode::nsEUCJPToUnicode() 
: nsMultiTableDecoderSupport(4,
                            (uRange *) &g_EUCJPRanges,
                            (uShiftTable**) &g_EUCJPShiftTable, 
                            (uMappingTable**) &g_EUCJPMappingTable)
{
}

nsresult nsEUCJPToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsEUCJPToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsEUCJPToUnicode::GetMaxLength(const char * aSrc, 
                                             PRInt32 aSrcLength, 
                                             PRInt32 * aDestLength)
{
  // worst case
  *aDestLength = aSrcLength;
  return NS_OK;
}
