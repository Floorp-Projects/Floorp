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

#ifndef nsIDOMEventReceiver_h__
#define nsIDOMEventReceiver_h__

#include "nsIDOMEventTarget.h"

class nsIDOMEventListener;
class nsIDOMMouseListener;
class nsIDOMMouseMotionListener;
class nsIDOMKeyListener;
class nsIDOMFocusListener;
class nsIDOMLoadListener;
class nsIDOMDragListener;
class nsIEventListenerManager;
class nsIDOMEvent;

/*
 * DOM event source class.  Object that allow event registration and
 * distribution from themselves implement this interface.
 */

#define NS_IDOMEVENTRECEIVER_IID \
{ /* e1dbcba0-fb38-11d1-bd87-00805f8ae3f4 */ \
0xe1dbcba0, 0xfb38, 0x11d1, \
{0xbd, 0x87, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMEventReceiver : public nsIDOMEventTarget
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMEVENTRECEIVER_IID)

  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID) = 0;
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID) = 0;
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) = 0;
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent) = 0;
};
#endif // nsIDOMEventReceiver_h__
