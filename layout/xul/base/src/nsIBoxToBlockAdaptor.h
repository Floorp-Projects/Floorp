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
 * Author:
 * Eric D Vaughan
 *
 * Contributor(s): 
 */

#ifndef nsIBoxToBlockAdaptor_h___
#define nsIBoxToBlockAdaptor_h___

#include "nsISupports.h"

class nsIPresContext;

// {162F6B61-F926-11d3-BA06-001083023C1E}
#define NS_IBOX_TO_BLOCK_ADAPTOR_IID { 0x162f6b61, 0xf926, 0x11d3, { 0xba, 0x6, 0x0, 0x10, 0x83, 0x2, 0x3c, 0x1e } }


class nsIBoxToBlockAdaptor : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_IBOX_TO_BLOCK_ADAPTOR_IID; return iid; }

  NS_IMETHOD Recycle(nsIPresShell* aPresShell)=0;
};

#endif

