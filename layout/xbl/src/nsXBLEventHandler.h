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

#ifndef nsXBLEventHandler_h__
#define nsXBLEventHandler_h__

#include "nsIDOMEventReceiver.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIDOMFormListener.h"

class nsIXBLBinding;
class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIAtom;
class nsIController;
class nsIXBLPrototypeHandler;

class nsXBLEventHandler : public nsIDOMKeyListener, 
                          public nsIDOMMouseListener,
                          public nsIDOMMenuListener,
                          public nsIDOMFocusListener,
                          public nsIDOMScrollListener,
                          public nsIDOMFormListener
{
public:
  nsXBLEventHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler, const nsString& aEventName);
  virtual ~nsXBLEventHandler();
  
  NS_IMETHOD BindingAttached();
  NS_IMETHOD BindingDetached();

  // nsIDOMetc.
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  
  virtual nsresult KeyUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult KeyDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult KeyPress(nsIDOMEvent* aMouseEvent);
   
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);
  
  virtual nsresult Focus(nsIDOMEvent* aMouseEvent);
  virtual nsresult Blur(nsIDOMEvent* aMouseEvent);

  // menu
  NS_IMETHOD Create(nsIDOMEvent* aEvent);
  NS_IMETHOD Close(nsIDOMEvent* aEvent);
  NS_IMETHOD Destroy(nsIDOMEvent* aEvent);
  NS_IMETHOD Action(nsIDOMEvent* aEvent);
  NS_IMETHOD Broadcast(nsIDOMEvent* aEvent);
  NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent);

  // scroll
  NS_IMETHOD Overflow(nsIDOMEvent* aEvent);
  NS_IMETHOD Underflow(nsIDOMEvent* aEvent);
  NS_IMETHOD OverflowChanged(nsIDOMEvent* aEvent);

  // form
  virtual nsresult Submit(nsIDOMEvent* aEvent);
  virtual nsresult Reset(nsIDOMEvent* aEvent);
  virtual nsresult Change(nsIDOMEvent* aEvent);
  virtual nsresult Select(nsIDOMEvent* aEvent);
  virtual nsresult Input(nsIDOMEvent* aEvent);
  
  NS_DECL_ISUPPORTS

public:
  void SetNextHandler(nsXBLEventHandler* aHandler) {
    mNextHandler = aHandler;
  }

  void RemoveEventHandlers();

  void MarkForDeath() {
    if (mNextHandler) mNextHandler->MarkForDeath(); mProtoHandler = nsnull; mEventReceiver = nsnull;
  }

protected:
  NS_IMETHOD GetController(nsIController** aResult);
  NS_IMETHOD ExecuteHandler(const nsAReadableString & aEventName, nsIDOMEvent* aEvent);

  static PRUint32 gRefCnt;
  static nsIAtom* kKeyAtom;
  static nsIAtom* kKeyCodeAtom;
  static nsIAtom* kCharCodeAtom;
  static nsIAtom* kActionAtom;
  static nsIAtom* kCommandAtom;
  static nsIAtom* kClickCountAtom;
  static nsIAtom* kButtonAtom;
  static nsIAtom* kBindingAttachedAtom;
  static nsIAtom* kBindingDetachedAtom;
  static nsIAtom* kModifiersAtom;

  static nsresult GetTextData(nsIContent *aParent, nsString& aResult);

protected:
  nsIDOMEventReceiver* mEventReceiver; // Both of these refs are weak.
  nsCOMPtr<nsIXBLPrototypeHandler> mProtoHandler;

  nsAutoString mEventName;

  nsXBLEventHandler* mNextHandler; // Handlers are chained for easy unloading later.
};

extern nsresult
NS_NewXBLEventHandler(nsIDOMEventReceiver* aEventReceiver, nsIXBLPrototypeHandler* aHandlerElement, 
                      const nsString& aEventName,
                      nsXBLEventHandler** aResult);


#endif
