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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMXULCommandDispatcher_h__
#define nsIDOMXULCommandDispatcher_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIController;
class nsIDOMElement;
class nsIDOMWindow;

#define NS_IDOMXULCOMMANDDISPATCHER_IID \
 { 0xf3c50361, 0x14fe, 0x11d3, \
		{ 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } } 

class nsIDOMXULCommandDispatcher : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULCOMMANDDISPATCHER_IID; return iid; }

  NS_IMETHOD    GetFocusedElement(nsIDOMElement** aFocusedElement)=0;
  NS_IMETHOD    SetFocusedElement(nsIDOMElement* aFocusedElement)=0;

  NS_IMETHOD    GetFocusedWindow(nsIDOMWindow** aFocusedWindow)=0;
  NS_IMETHOD    SetFocusedWindow(nsIDOMWindow* aFocusedWindow)=0;

  NS_IMETHOD    AddCommand(nsIDOMElement* aListener)=0;

  NS_IMETHOD    RemoveCommand(nsIDOMElement* aListener)=0;

  NS_IMETHOD    UpdateCommands()=0;

  NS_IMETHOD    GetController(nsIController** aReturn)=0;

  NS_IMETHOD    SetController(nsIController* aController)=0;
};


#define NS_DECL_IDOMXULCOMMANDDISPATCHER   \
  NS_IMETHOD    GetFocusedElement(nsIDOMElement** aFocusedElement);  \
  NS_IMETHOD    SetFocusedElement(nsIDOMElement* aFocusedElement);  \
  NS_IMETHOD    GetFocusedWindow(nsIDOMWindow** aFocusedWindow);  \
  NS_IMETHOD    SetFocusedWindow(nsIDOMWindow* aFocusedWindow);  \
  NS_IMETHOD    AddCommand(nsIDOMElement* aListener);  \
  NS_IMETHOD    RemoveCommand(nsIDOMElement* aListener);  \
  NS_IMETHOD    UpdateCommands();  \
  NS_IMETHOD    GetController(nsIController** aReturn);  \
  NS_IMETHOD    SetController(nsIController* aController);  \



#define NS_FORWARD_IDOMXULCOMMANDDISPATCHER(_to)  \
  NS_IMETHOD    GetFocusedElement(nsIDOMElement** aFocusedElement) { return _to GetFocusedElement(aFocusedElement); } \
  NS_IMETHOD    SetFocusedElement(nsIDOMElement* aFocusedElement) { return _to SetFocusedElement(aFocusedElement); } \
  NS_IMETHOD    GetFocusedWindow(nsIDOMWindow** aFocusedWindow) { return _to GetFocusedWindow(aFocusedWindow); } \
  NS_IMETHOD    SetFocusedWindow(nsIDOMWindow* aFocusedWindow) { return _to SetFocusedWindow(aFocusedWindow); } \
  NS_IMETHOD    AddCommand(nsIDOMElement* aListener) { return _to AddCommand(aListener); }  \
  NS_IMETHOD    RemoveCommand(nsIDOMElement* aListener) { return _to RemoveCommand(aListener); }  \
  NS_IMETHOD    UpdateCommands() { return _to UpdateCommands(); }  \
  NS_IMETHOD    GetController(nsIController** aReturn) { return _to GetController(aReturn); }  \
  NS_IMETHOD    SetController(nsIController* aController) { return _to SetController(aController); }  \


extern "C" NS_DOM nsresult NS_InitXULCommandDispatcherClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULCommandDispatcher(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULCommandDispatcher_h__
