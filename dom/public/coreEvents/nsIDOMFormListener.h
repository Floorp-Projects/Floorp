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


#ifndef nsIDOMFormListener_h__
#define nsIDOMFormListener_h__

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

/*
 * Form submit/reset listener
 *
 */
#define NS_IDOMFORMLISTENER_IID \
{ /* 566c3f80-28ab-11d2-bd89-00805f8ae3f4 */ \
0x566c3f80, 0x28ab, 0x11d2, \
{0xbd, 0x89, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMFormListener : public nsIDOMEventListener {

public:
   NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMFORMLISTENER_IID)
  /**
  * Processes a form submit event
  * @param aEvent @see nsIDOMEvent.h 
  * @returns whether the event was consumed or ignored. @see nsresult
  */
  NS_IMETHOD Submit(nsIDOMEvent* aEvent) = 0;

  /**
   * Processes a form reset event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD Reset(nsIDOMEvent* aEvent) = 0;

  /**
   * Processes a form change event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD Change(nsIDOMEvent* aEvent) = 0;

  /**
   * Processes a form select event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD Select(nsIDOMEvent* aEvent) = 0;
  
  /**
   * Processes a form input event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  NS_IMETHOD Input(nsIDOMEvent* aEvent) = 0;

};

#endif // nsIDOMFormListener_h__
