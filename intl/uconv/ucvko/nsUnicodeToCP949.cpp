/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nsUnicodeToCP949.h"
#include "nsUCvKODll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRUint16 gAsciiShiftTable[] =  {
  0, u1ByteCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};

static const PRUint16 gKSC5601ShiftTable[] =  {
  0, u2BytesGRCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};
static const PRUint16 gCP949ShiftTable[] =  {
  0, u2BytesCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};

// Unicode Hangul syllables (not enumerated in KS X 1001) to CP949 : 8822 of them
static const PRUint16 g_ufCP949NoKSCHangulMapping[] = {
#include "u20cp949hangul.uf"
};



static const PRUint16 *g_CP949MappingTable[3] = {
  g_AsciiMapping,
  g_ufKSC5601Mapping,
  g_ufCP949NoKSCHangulMapping
};

static const PRUint16 *g_CP949ShiftTable[3] =  {
  gAsciiShiftTable,
  gKSC5601ShiftTable,
  gCP949ShiftTable
};

//----------------------------------------------------------------------
// Class nsUnicodeToEUCKR [implementation]

nsUnicodeToCP949::nsUnicodeToCP949() 
: nsMultiTableEncoderSupport(3,
                        (uShiftTable**) g_CP949ShiftTable, 
                        (uMappingTable**) g_CP949MappingTable)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToCP949::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength * 2; 
  return NS_OK;
}
