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

#ifndef nsIDOMEventListener_h__
#define nsIDOMEventListener_h__

#include "nsIDOMEvent.h"
#include "nsISupports.h"

/*
 * Event listener interface.
 */

#define NS_IDOMEVENTLISTENER_IID \
{ /* df31c120-ded6-11d1-bd85-00805f8ae3f4 */ \
0xdf31c120, 0xded6, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMEventListener : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEVENTLISTENER_IID; return iid; }

 /**
  * Processes all events excepting mouse and key events. 
  * @param anEvent the event to process. @see nsIDOMEvent.h for event types.
  */

  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) = 0;

};

#endif // nsIDOMEventListener_h__
