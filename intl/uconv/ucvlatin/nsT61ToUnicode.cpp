/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsT61ToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_T61MappingTable[] = {
#include "t61.ut"
};


static const PRInt16 g_T61ShiftTable[] =  {
    3, uMultibytesCharset,  
    ShiftCell(u1ByteChar,   1, 0x00, 0xBF, 0x00, 0x00, 0x00, 0xBF),
    ShiftCell(u1ByteChar,   1, 0xD0, 0xFF, 0x00, 0xD0, 0x00, 0xFF),
    ShiftCell(u2BytesChar,  2, 0xC0, 0xCF, 0xC0, 0x41, 0xCF, 0x7A)
};

//----------------------------------------------------------------------
// Class nsT61ToUnicode [implementation]

nsT61ToUnicode::nsT61ToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_T61ShiftTable, 
                        (uMappingTable*) &g_T61MappingTable)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

NS_IMETHODIMP nsT61ToUnicode::GetMaxLength(const char * aSrc, 
                                            PRInt32 aSrcLength, 
                                            PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}
