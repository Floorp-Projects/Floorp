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

#include "nsUnicodeToJohab.h"
#include "nsUCvKODll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRUint16 gAsciiShiftTable[] =  {
  0, u1ByteCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};

static const PRUint16 gJohabSymbolShiftTable[] =  {
  0, uJohabSymbolCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};
static const PRUint16 gJohabHangulShiftTable[] =  {
  0, uJohabHangulCharset,  
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};

static const PRUint16 *g_JohabMappingTable[3] = {
  g_AsciiMapping,
  g_HangulNullMapping,
  g_ufKSC5601Mapping
};

static const PRUint16 *g_JohabShiftTable[3] =  {
  gAsciiShiftTable,
  gJohabHangulShiftTable,
  gJohabSymbolShiftTable
};

//----------------------------------------------------------------------
// Class nsUnicodeToJohab [implementation]

nsUnicodeToJohab::nsUnicodeToJohab() 
: nsMultiTableEncoderSupport(3,
                        (uShiftTable**) g_JohabShiftTable, 
                        (uMappingTable**) g_JohabMappingTable)
{
}
NS_IMETHODIMP nsUnicodeToJohab::GetMaxLength(const PRUnichar* aSrc,
                                             PRInt32 aSrcLength,
                                             PRInt32 * aDestLength)
{ 
  *aDestLength = 2 * aSrcLength;
  return NS_OK;
}
