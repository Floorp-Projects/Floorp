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

#ifndef nsUCvCnCID_h___
#define nsUCvCnCID_h___

#include "nsISupports.h"

// Class ID for our GB2312ToUnicode charset converter
// {379C2774-EC77-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kGB2312ToUnicodeCID,
  0x379c2774, 0xec77, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

// Class ID for our UnicodeToGB2312 charset converter
// {379C2777-EC77-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToGB2312CID, 
  0x379c2777, 0xec77, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);

#endif /* nsUCvCnCID_h___ */
