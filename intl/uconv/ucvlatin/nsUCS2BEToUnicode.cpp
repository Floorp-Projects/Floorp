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

#include "nsUCS2BEToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_UCS2BEMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0xFFFF, 0x0000
};

static PRInt16 g_UCS2BEShiftTable[] =  {
  0, u2BytesCharset, 
  ShiftCell(0,       0, 0, 0, 0, 0, 0, 0), 
};

//----------------------------------------------------------------------
// Class nsUCS2BEToUnicode [implementation]

nsUCS2BEToUnicode::nsUCS2BEToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_UCS2BEShiftTable, 
                        (uMappingTable*) &g_UCS2BEMappingTable)
{
}

nsresult nsUCS2BEToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUCS2BEToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsUCS2BEToUnicode::GetMaxLength(const char * aSrc, 
                                            PRInt32 aSrcLength, 
                                            PRInt32 * aDestLength)
{
  if(0 == (aSrcLength % 2)) 
  {
    *aDestLength = aSrcLength / 2;
    return NS_OK_UENC_EXACTLENGTH;
  } else {
    *aDestLength = (aSrcLength  + 1) / 2;
    return NS_OK;
  }
}
