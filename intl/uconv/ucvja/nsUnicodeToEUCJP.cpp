/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsUnicodeToEUCJP.h"
#include "nsUCVJADll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

// Shift Table
static const PRInt16 g0201ShiftTable[] =  {
        2, uMultibytesCharset,
        ShiftCell(u1ByteChar,           1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
static const PRInt16 g0208ShiftTable[] =  {
        0, u2BytesGRCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g0212ShiftTable[] =  {
        0, u2BytesGRPrefix8FCharset, 
        ShiftCell(0,0,0,0,0,0,0,0)
};
#define SIZE_OF_TABLES 5
static const PRInt16 *gShiftTables[SIZE_OF_TABLES] =  {
    g0208ShiftTable,
    g0208ShiftTable,
    g0201ShiftTable,
    g0201ShiftTable,
    g0212ShiftTable
};

static const PRUint16 *gMappingTables[SIZE_OF_TABLES] = {
    g_uf0208Mapping,
    g_uf0208extMapping,
    g_uf0201Mapping,
    g_uf0201Mapping,
    g_uf0212Mapping
};

//----------------------------------------------------------------------
// Class nsUnicodeToEUCJP [implementation]

nsUnicodeToEUCJP::nsUnicodeToEUCJP() 
: nsMultiTableEncoderSupport(SIZE_OF_TABLES,
                         (uShiftTable**) gShiftTables, 
                         (uMappingTable**) gMappingTables)
{
}

nsresult nsUnicodeToEUCJP::CreateInstance(nsISupports ** aResult) 
{
  nsIUnicodeEncoder *p = new nsUnicodeToEUCJP();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToEUCJP::GetMaxLength(const PRUnichar * aSrc, 
                                             PRInt32 aSrcLength,
                                             PRInt32 * aDestLength)
{
  *aDestLength = 3*aSrcLength;
  return NS_OK;
}
