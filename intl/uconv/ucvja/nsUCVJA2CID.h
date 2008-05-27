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

#ifndef nsUCVJA2CID_h___
#define nsUCVJA2CID_h___

#include "nsISupports.h"

// Class ID for our EUCJPToUnicode charset converter
// {3F6FE6A1-AC0A-11d2-B3AE-00805F8A6670}
#define NS_EUCJPTOUNICODE_CID \
  {0x3f6fe6a1, 0xac0a, 0x11d2, {0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

// Class ID for our ISO2022JPToUnicode charset converter
// {3F6FE6A2-AC0A-11d2-B3AE-00805F8A6670}
#define NS_ISO2022JPTOUNICODE_CID \
  {0x3f6fe6a2, 0xac0a, 0x11d2, {0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

// Class ID for our UnicodeToEUCJP charset converter
// {45C23A20-D71C-11d2-8AAC-00600811A836}
#define NS_UNICODETOEUCJP_CID \
  {0x45c23a20, 0xd71c, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}} 

// Class ID for our UnicodeToISO2022JP charset converter
// {4F76E100-D71C-11d2-8AAC-00600811A836}
#define NS_UNICODETOISO2022JP_CID \
  {0x4f76e100, 0xd71c, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}} 

// Class ID for our UnicodeToJISx0201 charset converter
// {BA615191-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOJISX0201_CID \
  {0xba615191, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

#endif /* nsUCVJA2CID_h___ */
