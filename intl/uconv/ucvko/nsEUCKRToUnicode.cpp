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

#include "nsEUCKRToUnicode.h"
#include "nsUCvKODll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

#ifdef MS_EUCKR
static const PRUint16 g_EUCKRMappingTable[] = {
#include "u20ksc.ut"
};

static const PRUint16 g_EUCKRShiftTable[] =  {
  0, u2BytesCharset,  
  ShiftCell(0,  0, 0, 0, 0, 0, 0, 0)
};

static const uRange g_EUCKRRanges[] = {
  { 0x00, 0x7E },
  { 0x81, 0xFE }
};
#else

static const PRUint16 g_EUCKRShiftTable[] =  {
  0, u2BytesGRCharset,  
  ShiftCell(0,  0, 0, 0, 0, 0, 0, 0)
};

static const uRange g_EUCKRRanges[] = {
  { 0x00, 0x7E },
  { 0xA4, 0xA4 },   // 8byte seq. for Hangul syllables not available
                    // in pre-composed form in KS X 1001
  { 0xA1, 0xFE }
};
#endif

static const PRUint16 g_DecomposedHangulShiftTable[] =  {
  0, uDecomposedHangulCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};



static const PRUint16 *g_EUCKRShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_DecomposedHangulShiftTable,
  g_EUCKRShiftTable
};

static const PRUint16 *g_EUCKRMappingTableSet [] ={
  g_AsciiMapping,
  g_HangulNullMapping,
  g_utKSC5601Mapping
};


//----------------------------------------------------------------------
// Class nsEUCKRToUnicode [implementation]

nsEUCKRToUnicode::nsEUCKRToUnicode() 
: nsMultiTableDecoderSupport(3,
                        (uRange*) &g_EUCKRRanges,
                        (uShiftTable**) &g_EUCKRShiftTableSet, 
                        (uMappingTable**) &g_EUCKRMappingTableSet)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsEUCKRToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
