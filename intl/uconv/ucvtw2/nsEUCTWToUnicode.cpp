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

#include "nsEUCTWToUnicode.h"
#include "nsUCvTW2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 g_CNS1ShiftTable[] =  {
  0, u2BytesGRCharset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};


static const PRInt16 g_CNS2ShiftTable[] =  {
  0, u2BytesGRPrefix8EA2Charset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};
static const PRInt16 g_CNS3ShiftTable[] =  {
  0, u2BytesGRPrefix8EA3Charset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};
static const PRInt16 g_CNS4ShiftTable[] =  {
  0, u2BytesGRPrefix8EA4Charset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};
static const PRInt16 g_CNS5ShiftTable[] =  {
  0, u2BytesGRPrefix8EA5Charset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};
static const PRInt16 g_CNS6ShiftTable[] =  {
  0, u2BytesGRPrefix8EA6Charset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};
static const PRInt16 g_CNS7ShiftTable[] =  {
  0, u2BytesGRPrefix8EA7Charset,  
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};


static const PRInt16 *g_EUCTWShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_CNS1ShiftTable,
  g_CNS2ShiftTable,
  g_CNS3ShiftTable,
  g_CNS4ShiftTable,
  g_CNS5ShiftTable,
  g_CNS6ShiftTable,
  g_CNS7ShiftTable
};

static const PRUint16 *g_EUCTWMappingTableSet [] ={
  g_ASCIIMappingTable,
  g_utCNS1MappingTable,
  g_utCNS2MappingTable,
  g_utCNS3MappingTable,
  g_utCNS4MappingTable,
  g_utCNS5MappingTable,
  g_utCNS6MappingTable,
  g_utCNS7MappingTable
};

static const uRange g_EUCTWRanges[] = {
  { 0x00, 0x7E },
  { 0xA1, 0xFE },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E },
  { 0x8E, 0x8E }
};


//----------------------------------------------------------------------
// Class nsEUCTWToUnicode [implementation]

nsEUCTWToUnicode::nsEUCTWToUnicode() 
: nsMultiTableDecoderSupport(8, 
                        (uRange*) &g_EUCTWRanges, 
                        (uShiftTable**) &g_EUCTWShiftTableSet, 
                        (uMappingTable**) &g_EUCTWMappingTableSet)
{
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
