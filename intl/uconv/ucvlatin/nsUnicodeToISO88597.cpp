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

#include "nsUnicodeToISO88597.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_ISO88597MappingTable[] = {
#include "8859-7.uf"
};

static PRInt16 g_ISO88597ShiftTable[] =  {
  1, u1ByteCharset ,
  ShiftCell(0,0,0,0,0,0,0,0)
};

//----------------------------------------------------------------------
// Class nsUnicodeToISO88597 [implementation]

nsUnicodeToISO88597::nsUnicodeToISO88597() 
: nsTableEncoderSupport((uShiftTable*) &g_ISO88597ShiftTable, 
                        (uMappingTable*) &g_ISO88597MappingTable)
{
}

nsresult nsUnicodeToISO88597::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsUnicodeToISO88597();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToISO88597::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}
