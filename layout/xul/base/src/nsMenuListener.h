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
#ifndef nsMenuListener_h__
#define nsMenuListener_h__

#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMEventReceiver.h"

class nsIMenuParent;
class nsIPresContext;

/** editor Implementation of the DragListener interface
 */
class nsMenuListener : public nsIDOMKeyListener, public nsIDOMFocusListener, public nsIDOMMouseListener
{
public:
  nsMenuListener(nsIMenuParent* aMenuParent);
  
  virtual ~nsMenuListener();
   
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);
  
  NS_IMETHOD KeyUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aMouseEvent);
  
  NS_IMETHOD Focus(nsIDOMEvent* aEvent);
  NS_IMETHOD Blur(nsIDOMEvent* aEvent);
  
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
  
  NS_DECL_ISUPPORTS

protected:
  nsIMenuParent* mMenuParent;    // The outermost object capturing events (either a menu bar or menupopup).
};


#endif
