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

#ifndef nsUCvCnCID_h___
#define nsUCvCnCID_h___

#include "nsISupports.h"

// Class ID for our GB2312ToUnicode charset converter// {379C2774-EC77-11d2-8AAC-00600811A836}
#define NS_GB2312TOUNICODE_CID \
  { 0x379c2774, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our ISO2022CNToUnicode charset converter
// {BA615199-1DFA-11d3-B3BF-00805F8A6670}
#define NS_ISO2022CNTOUNICODE_CID \
  { 0xba615199, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}
 
// Class ID for our HZToUnicode charset converter
// {BA61519A-1DFA-11d3-B3BF-00805F8A6670}
#define NS_HZTOUNICODE_CID \
  { 0xba61519a, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our GBKToUnicode charset converter
// {BA61519E-1DFA-11d3-B3BF-00805F8A6670}
#define NS_GBKTOUNICODE_CID \
  { 0xba61519e, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGB2312 charset converter
// {379C2777-EC77-11d2-8AAC-00600811A836}
#define NS_UNICODETOGB2312_CID \
  { 0x379c2777, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToGB2312GL charset converter
// {BA615196-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOGB2312GL_CID \
  { 0xba615196, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGBK charset converter
// {BA61519B-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOGBK_CID \
  { 0xba61519b, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGBKNoAscii charset converter
// {af0de730-1dd1-11b2-a8a1-b60772efe214}
#define NS_UNICODETOGBKNOASCII_CID \
  { 0xaf0de730, 0x1dd1, 0x11b2, {0xa8, 0xa1, 0xb6, 0x07, 0x72, 0xef, 0xe2, 0x14}}

// Class ID for our UnicodeToISO2022CN charset converter
// {BA61519C-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOISO2022CN_CID \
  { 0xba61519c, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToHZ charset converter
// {BA61519D-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOHZ_CID \
  { 0xba61519d, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our CP936ToUnicode charset converter
// {9416BFC0-1F93-11d3-B3BF-00805F8A6670}
#define NS_CP936TOUNICODE_CID \
  { 0x9416bfc0, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCP936 charset converter
// {9416BFC1-1F93-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOCP936_CID \
  { 0x9416bfc1, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGB18030 charset converter
// {A59DA932-4091-11d5-A145-005004832142}
#define NS_UNICODETOGB18030_CID \
  { 0xa59da932, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }

// Class ID for our UnicodeToGB18030Font0 charset converter
// {A59DA933-4091-11d5-A145-005004832142}
#define NS_UNICODETOGB18030Font0_CID \
  { 0xa59da933, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }

// Class ID for our UnicodeToGB18030Font1 charset converter
// {A59DA934-4091-11d5-A145-005004832142}
#define NS_UNICODETOGB18030Font1_CID \
  { 0xa59da934, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }

// Class ID for our GBKToUnicode charset converter
// {A59DA935-4091-11d5-A145-005004832142}
#define NS_GB18030TOUNICODE_CID \
  { 0xa59da935, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }

#endif /* nsUCvCnCID_h___ */
