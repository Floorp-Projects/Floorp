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

#include "nsUnicodeToEUCTW.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_ufCNS1MappingTable[] = {
#include "cns_1.uf"
};
static PRUint16 g_ufCNS2MappingTable[] = {
#include "cns_2.uf"
};

static PRUint16 g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

static PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g_CNS1ShiftTable[] =  {
  0, u2BytesGRCharset,
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};

static PRInt16 g_CNS2ShiftTable[] =  {
  0, u2BytesGRPrefix8EA2Charset,
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};


static PRInt16 *g_EUCTWShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_CNS1ShiftTable,
  g_CNS2ShiftTable
};

static PRUint16 *g_EUCTWMappingTableSet [] ={
  g_ASCIIMappingTable,
  g_ufCNS1MappingTable,
  g_ufCNS2MappingTable
};

//----------------------------------------------------------------------
// Class nsUnicodeToEUCTW [implementation]

nsUnicodeToEUCTW::nsUnicodeToEUCTW() 
: nsMultiTableEncoderSupport( 3,
                        (uShiftTable**) &g_EUCTWShiftTableSet, 
                        (uMappingTable**) &g_EUCTWMappingTableSet)
{
}

nsresult nsUnicodeToEUCTW::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToEUCTW();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToEUCTW::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 4 * aSrcLength;
  return NS_OK;
}
