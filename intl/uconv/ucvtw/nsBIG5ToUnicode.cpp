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

#include "nsBIG5ToUnicode.h"
#include "nsUCvTWDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRInt16 g_BIG5ShiftTable[] =  {
  0, u2BytesCharset,  
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 *g_BIG5ShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_BIG5ShiftTable
};

static const PRUint16 *g_BIG5MappingTableSet [] ={
  g_ASCIIMapping,
  g_utBIG5Mapping
};

static const uRange g_BIG5Ranges[] = {
  { 0x00, 0x7E },
  { 0x81, 0xFC }
};

//----------------------------------------------------------------------
// Class nsBIG5ToUnicode [implementation]

nsBIG5ToUnicode::nsBIG5ToUnicode() 
: nsMultiTableDecoderSupport(2, 
                            (uRange* ) &g_BIG5Ranges,
                            (uShiftTable**) &g_BIG5ShiftTableSet, 
                            (uMappingTable**) &g_BIG5MappingTableSet)
{
}


//----------------------------------------------------------------------
// Subclassing of nsMultiTableDecoderSupport class [implementation]

NS_IMETHODIMP nsBIG5ToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
