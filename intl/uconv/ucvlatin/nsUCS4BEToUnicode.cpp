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

#include "nsUCS4BEToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_UCS4BEMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0xFFFF, 0x0000
};

static const PRInt16 g_UCS4BEShiftTable[] =  {
  0, u4BytesCharset, 
  ShiftCell(0,       0, 0, 0, 0, 0, 0, 0), 
};

//----------------------------------------------------------------------
// Class nsUCS4BEToUnicode [implementation]

nsUCS4BEToUnicode::nsUCS4BEToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_UCS4BEShiftTable, 
                        (uMappingTable*) &g_UCS4BEMappingTable)
{
}

nsresult nsUCS4BEToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUCS4BEToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsUCS4BEToUnicode::GetMaxLength(const char * aSrc, 
                                            PRInt32 aSrcLength, 
                                            PRInt32 * aDestLength)
{
  if(0 == (aSrcLength % 4)) 
  {
    *aDestLength = aSrcLength / 4;
    return NS_OK_UENC_EXACTLENGTH;
  } else {
    *aDestLength = (aSrcLength / 4) + 1;
    return NS_OK;
  }
}
