/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
  virtual nsresult Submit(nsIDOMEvent* aEvent) = 0;

  /**
   * Processes a form reset event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  virtual nsresult Reset(nsIDOMEvent* aEvent) = 0;

  /**
   * Processes a form change event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  virtual nsresult Change(nsIDOMEvent* aEvent) = 0;

  /**
   * Processes a form select event
   * @param aEvent @see nsIDOMEvent.h 
   * @returns whether the event was consumed or ignored. @see nsresult
   */
  virtual nsresult Select(nsIDOMEvent* aEvent) = 0;

};

#endif // nsIDOMFormListener_h__
