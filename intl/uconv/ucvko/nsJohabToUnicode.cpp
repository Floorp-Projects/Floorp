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

#include "nsJohabToUnicode.h"
#include "nsUCvKODll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static const PRUint16 g_ASCIIShiftTable[] =  {
  0, u1ByteCharset,
  ShiftCell(0,0,0,0,0,0,0,0)
};

static const PRUint16 g_JOHABHangulShiftTable[] =  {
  0, uJohabHangulCharset,
  ShiftCell(0,  0, 0, 0, 0, 0, 0, 0)
};

static const PRUint16 g_JOHABSymbolShiftTable[] =  {
  0, uJohabSymbolCharset,
  ShiftCell(0,  0, 0, 0, 0, 0, 0, 0)
};

static const uRange g_JOHABRanges[] = {
  { 0x00, 0x7E },
  { 0x84, 0xD3 },
  { 0xD8, 0xDE },
  { 0xE0, 0xF9 }
};


static const PRUint16 *g_JOHABShiftTableSet [] = {
  g_ASCIIShiftTable,
  g_JOHABHangulShiftTable,
  g_JOHABSymbolShiftTable,
  g_JOHABSymbolShiftTable
};

static const PRUint16 *g_JOHABMappingTableSet [] ={
  g_AsciiMapping,
  g_HangulNullMapping,
  g_utKSC5601Mapping,
  g_utKSC5601Mapping
};


//----------------------------------------------------------------------
// Class nsJohabToUnicode [implementation]

nsJohabToUnicode::nsJohabToUnicode() 
: nsMultiTableDecoderSupport(4,
                        (uRange*) &g_JOHABRanges,
                        (uShiftTable**) &g_JOHABShiftTableSet, 
                        (uMappingTable**) &g_JOHABMappingTableSet)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsJohabToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}
