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

#ifndef nsUCVJACID_h___
#define nsUCVJACID_h___

#include "nsISupports.h"

// Class ID for our SJIS2Unicode charset converter
// {0E6892C1-A9AD-11d2-B3AE-00805F8A6670}
#define NS_SJISTOUNICODE_CID \
  {0xe6892c1, 0xa9ad, 0x11d2, {0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

// Class ID for our UnicodeToSJIS charset converter
// {E28AB250-D66D-11d2-8AAC-00600811A836}
#define NS_UNICODETOSJIS_CID \
  {0xe28ab250, 0xd66d, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}} 

// Class ID for our CP932ToUnicode charset converter
// {9416BFBC-1F93-11d3-B3BF-00805F8A6670}
#define NS_CP932TOUNICODE_CID \
  {0x9416bfbc, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 


// Class ID for our UnicodeToCP932 charset converter
// {9416BFBD-1F93-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOCP932_CID \
  {0x9416bfbd, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 


#endif /* nsUCVJACID_h___ */
