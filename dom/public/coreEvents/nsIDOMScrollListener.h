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


#ifndef nsIDOMScrollListener_h__
#define nsIDOMScrollListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

// {00B04615-4BC4-11d4-BA11-001083023C1E}
#define NS_IDOMSCROLLLISTENER_IID \
{ 0xb04615, 0x4bc4, 0x11d4, { 0xba, 0x11, 0x0, 0x10, 0x83, 0x2, 0x3c, 0x1e } }

class nsIDOMScrollListener : public nsIDOMEventListener {

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMSCROLLLISTENER_IID)

  NS_IMETHOD Overflow(nsIDOMEvent* aEvent) = 0;
  NS_IMETHOD Underflow(nsIDOMEvent* aEvent) = 0;
  NS_IMETHOD OverflowChanged(nsIDOMEvent* aEvent) = 0;
};

#endif // nsIDOMScrollListener_h__
