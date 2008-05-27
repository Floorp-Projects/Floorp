/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUCvKOCID_h___
#define nsUCvKOCID_h___

#include "nsISupports.h"

// Class ID for our EUCKRToUnicode charset converter
// {379C2775-EC77-11d2-8AAC-00600811A836}
#define NS_EUCKRTOUNICODE_CID \
  { 0x379c2775, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our ISO2022KRToUnicode charset converter
// {BA61519f-1DFA-11d3-B3BF-00805F8A6670}
#define NS_ISO2022KRTOUNICODE_CID \
  { 0xba61519f, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToEUCKR charset converter
// {379C2778-EC77-11d2-8AAC-00600811A836}
#define NS_UNICODETOEUCKR_CID \
  { 0x379c2778, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToISO2022KR charset converter
// {BA6151A0-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOISO2022KR_CID \
  { 0xba6151a0, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCP949 charset converter
#define NS_UNICODETOCP949_CID \
  { 0x9416bfbe, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our CP949ToUnicode charset converter
#define NS_CP949TOUNICODE_CID \
  { 0x9416bfbf, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToJohab charset converter
// {D9B1F97E-CFA0-80b6-FB92-9972E48E3DCC}
#define NS_UNICODETOJOHAB_CID \
  { 0xd9b1f97e, 0xcfa0, 0x80b6,  {0xfb, 0x92, 0x99, 0x72, 0xe4, 0x8e, 0x3d, 0xcc}} 

// Class ID for our JohabToUnicode charset converter
// {D9B1F97F-CFA0-80b6-FB92-9972E48E3DCC}
#define NS_JOHABTOUNICODE_CID \
  { 0xd9b1f97f, 0xcfa0, 0x80b6,  {0xfb, 0x92, 0x99, 0x72, 0xe4, 0x8e, 0x3d, 0xcc}} 

// Class ID for our UnicodeToJamoTTF charset converter
// {47433d1d-d9a7-4954-994f-f7a05cf87c2e}
#define NS_UNICODETOJAMOTTF_CID \
  { 0x47433d1d, 0xd9a7, 0x4954, {0x99, 0x4f, 0xf7, 0xa0, 0x5c, 0xf8, 0x7c, 0x2e}}

#endif /* nsUCvKOCID_h___ */
