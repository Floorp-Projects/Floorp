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

#ifndef nsUCvTW2CID_h___
#define nsUCvTW2CID_h___

#include "nsISupports.h"

// Class ID for our EUCTWToUnicode charset converter
// {379C2771-EC77-11d2-8AAC-00600811A836}
#define NS_EUCTWTOUNICODE_CID \
  { 0x379c2771, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToEUCTW charset converter
// {379C2776-EC77-11d2-8AAC-00600811A836}
#define NS_UNICODETOEUCTW_CID \
  { 0x379c2776, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}


// Class ID for our UnicodeToCNS11643p1 charset converter
// {BA615197-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOCNS11643P1_CID \
  { 0xba615197, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCNS11643p2 charset converter
// {BA615198-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOCNS11643P2_CID \
  { 0xba615198, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCNS11643p3 charset converter
#define NS_UNICODETOCNS11643P3_CID \
  { 0x9416bfb5, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCNS11643p4 charset converter
#define NS_UNICODETOCNS11643P4_CID \
  { 0x9416bfb6, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCNS11643p5 charset converter
#define NS_UNICODETOCNS11643P5_CID \
  { 0x9416bfb7, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCNS11643p6 charset converter
#define NS_UNICODETOCNS11643P6_CID \
  { 0x9416bfb8, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCNS11643p7 charset converter
#define NS_UNICODETOCNS11643P7_CID \
  { 0x9416bfb9, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

#endif /* nsUCvTW2CID_h___ */
