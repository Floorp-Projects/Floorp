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

#ifndef nsIDOMWindow_h__
#define nsIDOMWindow_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

class nsIDOMNavigator;
class nsIDOMDocument;
class nsIDOMWindow;

#define NS_IDOMWINDOW_IID \
{ 0x6f76532c,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMWindow : public nsISupports {
public:

  NS_IMETHOD    GetWindow(nsIDOMWindow** aWindow)=0;

  NS_IMETHOD    GetSelf(nsIDOMWindow** aSelf)=0;

  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument)=0;

  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator)=0;

  NS_IMETHOD    GetOpener(nsIDOMWindow** aOpener)=0;

  NS_IMETHOD    Dump(const nsString& aStr)=0;

  NS_IMETHOD    Alert(const nsString& aStr)=0;

  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID)=0;

  NS_IMETHOD    ClearInterval(PRInt32 aTimerID)=0;

  NS_IMETHOD    SetTimeout(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn)=0;

  NS_IMETHOD    SetInterval(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn)=0;

  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn)=0;
};


#define NS_DECL_IDOMWINDOW   \
  NS_IMETHOD    GetWindow(nsIDOMWindow** aWindow);  \
  NS_IMETHOD    GetSelf(nsIDOMWindow** aSelf);  \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument);  \
  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator);  \
  NS_IMETHOD    GetOpener(nsIDOMWindow** aOpener);  \
  NS_IMETHOD    Dump(const nsString& aStr);  \
  NS_IMETHOD    Alert(const nsString& aStr);  \
  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID);  \
  NS_IMETHOD    ClearInterval(PRInt32 aTimerID);  \
  NS_IMETHOD    SetTimeout(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn);  \
  NS_IMETHOD    SetInterval(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn);  \
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn);  \



#define NS_FORWARD_IDOMWINDOW(superClass)  \
  NS_IMETHOD    GetWindow(nsIDOMWindow** aWindow) { return superClass::GetWindow(aWindow); } \
  NS_IMETHOD    GetSelf(nsIDOMWindow** aSelf) { return superClass::GetSelf(aSelf); } \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument) { return superClass::GetDocument(aDocument); } \
  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator) { return superClass::GetNavigator(aNavigator); } \
  NS_IMETHOD    GetOpener(nsIDOMWindow** aOpener) { return superClass::GetOpener(aOpener); } \
  NS_IMETHOD    Dump(const nsString& aStr) { return superClass::Dump(aStr); }  \
  NS_IMETHOD    Alert(const nsString& aStr) { return superClass::Alert(aStr); }  \
  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID) { return superClass::ClearTimeout(aTimerID); }  \
  NS_IMETHOD    ClearInterval(PRInt32 aTimerID) { return superClass::ClearInterval(aTimerID); }  \
  NS_IMETHOD    SetTimeout(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn) { return superClass::SetTimeout(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    SetInterval(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn) { return superClass::SetInterval(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn) { return superClass::Open(cx, argv, argc, aReturn); }  \


extern nsresult NS_InitWindowClass(nsIScriptContext *aContext, nsIScriptGlobalObject *aGlobal);

extern "C" NS_DOM nsresult NS_NewScriptWindow(nsIScriptContext *aContext, nsIDOMWindow *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMWindow_h__
