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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#ifndef nsMenuBarListener_h__
#define nsMenuBarListener_h__

#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMEventReceiver.h"

class nsMenuBarFrame;
class nsIPresContext;
class nsIDOMKeyEvent;

/** editor Implementation of the DragListener interface
 */
class nsMenuBarListener : public nsIDOMKeyListener, public nsIDOMFocusListener, public nsIDOMMouseListener
{
public:
  /** default constructor
   */
  nsMenuBarListener(nsMenuBarFrame* aMenuBar);
  /** default destructor
   */
  virtual ~nsMenuBarListener();
   
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

  static nsresult GetMenuAccessKey(PRInt32* aAccessKey);
  
  NS_DECL_ISUPPORTS

protected:
  static void InitAccessKey();

  PRBool IsAccessKeyPressed(nsIDOMKeyEvent* event);

  nsMenuBarFrame* mMenuBarFrame; // The menu bar object.
  PRBool mAccessKeyDown;         // Whether or not the ALT key is currently down.
  static PRBool mAccessKeyFocuses; // Does the access key by itself focus the menubar?
  static PRInt32 mAccessKey;     // See nsIDOMKeyEvent.h for sample values
};


#endif
