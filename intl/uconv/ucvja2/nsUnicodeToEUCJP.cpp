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

#include "nsUnicodeToEUCJP.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

//----------------------------------------------------------------------
// Class nsUnicodeToEUCJP [implementation]

nsUnicodeToEUCJP::nsUnicodeToEUCJP() 
: nsTableEncoderSupport((uShiftTable*) NULL, 
                        (uMappingTable*) NULL)
{
}

nsresult nsUnicodeToEUCJP::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUnicodeToEUCJP();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToEUCJP::GetMaxLength(const PRUnichar * aSrc, 
                                             PRInt32 aSrcLength,
                                             PRInt32 * aDestLength)
{
  // XXX write me
  *aDestLength = aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}
