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
 * Gavin Ho
 */

#include "nsBIG5HKSCSToUnicode.h"
#include "nsUCvTWDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRInt16 g_BIG5HKSCSShiftTable[] =  {
  0, u2BytesCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 *g_BIG5HKSCSShiftTableSet [] = {

  g_ASCIIShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable
};

static const PRUint16 *g_BIG5HKSCSMappingTableSet [] ={
  g_ASCIIMapping,
  g_utBig5HKSCSMapping,
  g_utBIG5Mapping,
  g_utBig5HKSCSMapping,
  g_utBIG5Mapping,
  g_utBig5HKSCSMapping,
};

static const uRange g_BIG5HKSCSRanges[] = {
  { 0x00, 0x7E },
  { 0x81, 0xA0 },
  { 0xA1, 0xC6 },
  { 0xC6, 0xC8 },
  { 0xC9, 0xF9 },
  { 0xF9, 0xFE }
};

//----------------------------------------------------------------------
// Class nsBIG5HKSCSToUnicode [implementation]

nsBIG5HKSCSToUnicode::nsBIG5HKSCSToUnicode()
: nsMultiTableDecoderSupport(6,
                            (uRange* ) &g_BIG5HKSCSRanges,
                            (uShiftTable**) &g_BIG5HKSCSShiftTableSet,
                            (uMappingTable**) &g_BIG5HKSCSMappingTableSet)
{
}


//----------------------------------------------------------------------
// Subclassing of nsMultiTableDecoderSupport class [implementation]

NS_IMETHODIMP nsBIG5HKSCSToUnicode::GetMaxLength(const char * aSrc,
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
