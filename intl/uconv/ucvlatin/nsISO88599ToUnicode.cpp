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

#include "nsISO88599ToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static PRUint16 g_ISO88599MappingTable[] = {
#include "8859-9.ut"
};

static PRInt16 g_ISO88599ShiftTable[] =  {
  1, u1ByteCharset ,
  ShiftCell(0,0,0,0,0,0,0,0)
};

//----------------------------------------------------------------------
// Class nsISO88599ToUnicode [implementation]

nsISO88599ToUnicode::nsISO88599ToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_ISO88599ShiftTable, 
                        (uMappingTable*) &g_ISO88599MappingTable)
{
}

nsresult nsISO88599ToUnicode::CreateInstance(nsISupports ** aResult) 
{
  *aResult = new nsISO88599ToUnicode();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsISO88599ToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
