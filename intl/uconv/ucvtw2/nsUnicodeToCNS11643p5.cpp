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

#include "nsUnicodeToCNS11643p5.h"
#include "nsUCvTW2Dll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRInt16 g_ShiftTable[] =  {
  0, u2BytesCharset,
  ShiftCell(0, 0, 0, 0, 0, 0, 0, 0),
};

//----------------------------------------------------------------------
// Class nsUnicodeToCNS11643p5 [implementation]

nsUnicodeToCNS11643p5::nsUnicodeToCNS11643p5() 
: nsTableEncoderSupport((uShiftTable*) &g_ShiftTable, 
                        (uMappingTable*) &g_ufCNS5MappingTable)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToCNS11643p5::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 2 * aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}
