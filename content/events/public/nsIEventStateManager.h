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

#ifndef nsIEventStateManager_h__
#define nsIEventStateManager_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"
#include "nsVoidArray.h"

class nsIPresContext;
class nsIDOMEventState;
class nsIDOMEvent;

/*
 * Event listener manager interface.
 */
#define NS_IEVENTSTATEMANAGER_IID \
{ /* c8488290-07ce-11d2-bd88-00805f8ae3f4 */ \
0xc8488290, 0x07ce, 0x11d2, \
{0xbd, 0x88, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIEventStateManager : public nsISupports {

public:

 /**
  * Gets the current event target. 
  * @param
  */

  NS_IMETHOD GetEventTarget(nsISupports **aResult) = 0;

 /**
  * Gets the current event target. 
  * @param
  */

  NS_IMETHOD SetEventTarget(nsISupports *aSupports) = 0;

};

#endif // nsIEventStateManager_h__
