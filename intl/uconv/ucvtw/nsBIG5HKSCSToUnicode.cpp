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

#include "nsBIG5HKSCSToUnicode.h"
#include "nsUCvTWDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]


static const PRInt16 g_BIG5HKSCSShiftTable[] =  {
  0, u2BytesCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRInt16 *g_BIG5HKSCSShiftTableSet [] = {

  g_ASCIIShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable,
  g_BIG5HKSCSShiftTable
};

static const PRUint16 *g_BIG5HKSCSMappingTableSet [] ={
  g_ASCIIMapping,
  g_utBig5HKSCSMapping,
  g_utBIG5Mapping,
  g_utBig5HKSCSMapping,
  g_utBIG5Mapping,
  g_utBig5HKSCSMapping,
};

static const uRange g_BIG5HKSCSRanges[] = {
  { 0x00, 0x7E },
  { 0x81, 0xA0 },
  { 0xA1, 0xC6 },
  { 0xC6, 0xC8 },
  { 0xC9, 0xF9 },
  { 0xF9, 0xFE }
};

//----------------------------------------------------------------------
// Class nsBIG5HKSCSToUnicode [implementation]

nsBIG5HKSCSToUnicode::nsBIG5HKSCSToUnicode()
: nsMultiTableDecoderSupport(6,
                            (uRange* ) &g_BIG5HKSCSRanges,
                            (uShiftTable**) &g_BIG5HKSCSShiftTableSet,
                            (uMappingTable**) &g_BIG5HKSCSMappingTableSet)
{
}


//----------------------------------------------------------------------
// Subclassing of nsMultiTableDecoderSupport class [implementation]

NS_IMETHODIMP nsBIG5HKSCSToUnicode::GetMaxLength(const char * aSrc,
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
