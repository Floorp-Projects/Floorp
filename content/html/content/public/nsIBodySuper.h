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
 * Original Author: Daniel Glazman <glazman@netscape.com>
 *
 * Contributor(s): 
 */
#ifndef nsIBodySuper_h___
#define nsIBodySuper_h___

#include "nsISupports.h"

class nsISizeOfHandler;


// IID for the nsIBodySuper interface {61ed2ed8-1dd2-11b2-b51d-c0d42f7f5a56}

#define NS_IBODY_SUPER_IID     \
{0x61ed2ed8, 0x1dd2, 0x11b2, {0xb5, 0x1d, 0xc0, 0xd4, 0x2f, 0x7f, 0x5a, 0x56}}

class nsIBodySuper : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBODY_SUPER_IID)

  NS_IMETHOD RemoveBodyFixupRule(void) = 0;

};

#endif /* nsIBodySuper_h___ */



