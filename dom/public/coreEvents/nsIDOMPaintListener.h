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
#ifndef nsIDOMPaintListener_h__
#define nsIDOMPaintListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

/* a6cf906a-15b3-11d2-932e-00805f8add32 */
#define NS_IDOMPAINTLISTENER_IID \
 {0xa6cf906a, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/*
 * Paint event listener
 */
class nsIDOMPaintListener : public nsIDOMEventListener {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMPAINTLISTENER_IID)
  /**
  * Processes a paint event
  * @param aEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD Paint(nsIDOMEvent* aEvent) = 0;

  /**
  * Processes a resize event
  * @param aEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD Resize(nsIDOMEvent* aEvent) = 0;

  /**
  * Processes a scroll event
  * @param aEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD Scroll(nsIDOMEvent* aEvent) = 0;
};

#endif /* nsIDOMPaintListener_h__ */
