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

#include "nsGB2312ToUnicode.h"



//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};
static PRUint16 g_GB2312MappingTable[] = {
#include "gb2312.ut"
};


static PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g_GB2312ShiftTable[] =  {
  0, u2BytesGRCharset,  
  ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 *g_GB2312ShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_GB2312ShiftTable
};

static PRUint16 *g_GB2312MappingTableSet [] ={
  g_ASCIIMappingTable,
  g_GB2312MappingTable
};

static uRange g_GB2312Ranges[] = {
  { 0x00, 0x7E },
  { 0xA1, 0xFE }
};


//----------------------------------------------------------------------
// Class nsGB2312ToUnicode [implementation]

nsGB2312ToUnicode::nsGB2312ToUnicode() 
: nsMultiTableDecoderSupport(2, 
                        (uRange *) &g_GB2312Ranges,
                        (uShiftTable**) &g_GB2312ShiftTableSet, 
                        (uMappingTable**) &g_GB2312MappingTableSet)
{
}

nsresult nsGB2312ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsGB2312ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsGB2312ToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
