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

#include "nsUnicodeToBIG5NoAscii.h"
#include "nsUCvTWDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 gBig5ShiftTable[] =  {
  0, u2BytesCharset,
  ShiftCell(0,   0, 0, 0, 0, 0, 0, 0),
};
 

//----------------------------------------------------------------------
// Class nsUnicodeToBIG5NoAscii [implementation]

nsUnicodeToBIG5NoAscii::nsUnicodeToBIG5NoAscii() 
: nsTableEncoderSupport( (uShiftTable*) &gBig5ShiftTable, 
                        (uMappingTable*) &g_ufBig5Mapping)
{
}


//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToBIG5NoAscii::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 2 * aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}
NS_IMETHODIMP nsUnicodeToBIG5NoAscii::FillInfo(PRUint32 *aInfo)
{
  nsresult rv = nsTableEncoderSupport::FillInfo(aInfo); // call the super class
  if(NS_SUCCEEDED(rv))
  {
    // mark the first 128 bits as 0. 4 x 32 bits = 128 bits
    aInfo[0] = aInfo[1] = aInfo[2] = aInfo[3] = 0;
  }
  return rv;
}
