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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIPrivateDOMImplementation_h__
#define nsIPrivateDOMImplementation_h__

#include "nsISupports.h"

class nsIDocument;

/*
 * Event listener manager interface.
 */
#define NS_IPRIVATEDOMIMPLEMENTATION_IID \
{ /* d3205fb8-2652-11d4-ba06-0060b0fc76dd */ \
0xd3205fb8, 0x2652, 0x11d4, \
{0xba, 0x06, 0x00, 0x60, 0xb0, 0xfc, 0x76, 0xdd} }

class nsIPrivateDOMImplementation : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPRIVATEDOMIMPLEMENTATION_IID; return iid; }

  NS_IMETHOD Init(nsIDocument* aDoc) = 0;
};

NS_LAYOUT nsresult
NS_NewDOMImplementation(nsIDOMDOMImplementation** aInstancePtrResult);

#endif // nsIPrivateDOMImplementation_h__
