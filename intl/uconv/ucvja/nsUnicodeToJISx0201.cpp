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

#include "nsUnicodeToJISx0201.h"
#include "nsUCVJADll.h"

// Shift Table
static PRInt16 g0201ShiftTable[] =  {
        1, u1ByteCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

//----------------------------------------------------------------------
// Class nsUnicodeToJISx0201 [implementation]

nsUnicodeToJISx0201::nsUnicodeToJISx0201() 
: nsTableEncoderSupport((uShiftTable*)  g0201ShiftTable,
                        (uMappingTable*) g_uf0201Mapping)
{
}

nsresult nsUnicodeToJISx0201::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToJISx0201();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToJISx0201::GetMaxLength(const PRUnichar * aSrc, 
                                             PRInt32 aSrcLength,
                                             PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}
