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

#include "nsEUCTWToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

static PRUint16 g_CNS1MappingTable[] = {
#include "cns_1.ut"
};

static PRUint16 g_CNS2MappingTable[] = {
#include "cns_2.ut"
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
  g_CNS1MappingTable,
  g_CNS2MappingTable
};

static uRange g_EUCTWRanges[] = {
  { 0x00, 0x7E },
  { 0xA1, 0xFE },
  { 0x8E, 0x8E }
};


//----------------------------------------------------------------------
// Class nsEUCTWToUnicode [implementation]

nsEUCTWToUnicode::nsEUCTWToUnicode() 
: nsMultiTableDecoderSupport(3, 
                        (uRange*) &g_EUCTWRanges, 
                        (uShiftTable**) &g_EUCTWShiftTableSet, 
                        (uMappingTable**) &g_EUCTWMappingTableSet)
{
}

nsresult nsEUCTWToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsEUCTWToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsEUCTWToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
