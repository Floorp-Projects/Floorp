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
#ifndef nsMenuDismissalListener_h__
#define nsMenuDismissalListener_h__

#include "nsIWidget.h"
#include "nsIDOMMouseListener.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIDOMEventReceiver.h"

class nsMenuPopupFrame;
class nsIPresContext;
class nsIMenuParent;

/** editor Implementation of the DragListener interface
 */
class nsMenuDismissalListener : public nsIDOMMouseListener, public nsIMenuRollup, public nsIRollupListener
{
public:
  /** default constructor
   */
  nsMenuDismissalListener();
  
  /** default destructor
   */
  virtual ~nsMenuDismissalListener();
   
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; };
  
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; };
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIROLLUPLISTENER
  NS_DECL_NSIMENUROLLUP

  NS_IMETHOD EnableListener(PRBool aEnabled);
  void SetCurrentMenuParent(nsIMenuParent* aMenuParent);
  NS_IMETHOD Unregister();
  
protected:
  nsIMenuParent* mMenuParent;
  nsIWidget* mWidget;
  PRBool mEnabled;
};


#endif
