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
class nsIDOMWindowCollection;
class nsIDOMWindow;

#define NS_IDOMWINDOW_IID \
{ 0x6f76533d,  0xee43, 0x11d1, \
 { 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } } 

class nsIDOMWindow : public nsISupports {
public:

  NS_IMETHOD    GetWindow(nsIDOMWindow** aWindow)=0;

  NS_IMETHOD    GetSelf(nsIDOMWindow** aSelf)=0;

  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument)=0;

  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator)=0;

  NS_IMETHOD    GetParent(nsIDOMWindow** aParent)=0;

  NS_IMETHOD    GetTop(nsIDOMWindow** aTop)=0;

  NS_IMETHOD    GetClosed(PRBool* aClosed)=0;

  NS_IMETHOD    GetFrames(nsIDOMWindowCollection** aFrames)=0;

  NS_IMETHOD    GetOpener(nsIDOMWindow** aOpener)=0;
  NS_IMETHOD    SetOpener(nsIDOMWindow* aOpener)=0;

  NS_IMETHOD    GetStatus(nsString& aStatus)=0;
  NS_IMETHOD    SetStatus(const nsString& aStatus)=0;

  NS_IMETHOD    GetDefaultStatus(nsString& aDefaultStatus)=0;
  NS_IMETHOD    SetDefaultStatus(const nsString& aDefaultStatus)=0;

  NS_IMETHOD    GetName(nsString& aName)=0;
  NS_IMETHOD    SetName(const nsString& aName)=0;

  NS_IMETHOD    Dump(const nsString& aStr)=0;

  NS_IMETHOD    Alert(const nsString& aStr)=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Close()=0;

  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID)=0;

  NS_IMETHOD    ClearInterval(PRInt32 aTimerID)=0;

  NS_IMETHOD    SetTimeout(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn)=0;

  NS_IMETHOD    SetInterval(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn)=0;

  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc, nsIDOMWindow** aReturn)=0;
};


#define NS_DECL_IDOMWINDOW   \
  NS_IMETHOD    GetWindow(nsIDOMWindow** aWindow);  \
  NS_IMETHOD    GetSelf(nsIDOMWindow** aSelf);  \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument);  \
  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator);  \
  NS_IMETHOD    GetParent(nsIDOMWindow** aParent);  \
  NS_IMETHOD    GetTop(nsIDOMWindow** aTop);  \
  NS_IMETHOD    GetClosed(PRBool* aClosed);  \
  NS_IMETHOD    GetFrames(nsIDOMWindowCollection** aFrames);  \
  NS_IMETHOD    GetOpener(nsIDOMWindow** aOpener);  \
  NS_IMETHOD    SetOpener(nsIDOMWindow* aOpener);  \
  NS_IMETHOD    GetStatus(nsString& aStatus);  \
  NS_IMETHOD    SetStatus(const nsString& aStatus);  \
  NS_IMETHOD    GetDefaultStatus(nsString& aDefaultStatus);  \
  NS_IMETHOD    SetDefaultStatus(const nsString& aDefaultStatus);  \
  NS_IMETHOD    GetName(nsString& aName);  \
  NS_IMETHOD    SetName(const nsString& aName);  \
  NS_IMETHOD    Dump(const nsString& aStr);  \
  NS_IMETHOD    Alert(const nsString& aStr);  \
  NS_IMETHOD    Focus();  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Close();  \
  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID);  \
  NS_IMETHOD    ClearInterval(PRInt32 aTimerID);  \
  NS_IMETHOD    SetTimeout(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn);  \
  NS_IMETHOD    SetInterval(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn);  \
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc, nsIDOMWindow** aReturn);  \



#define NS_FORWARD_IDOMWINDOW(_to)  \
  NS_IMETHOD    GetWindow(nsIDOMWindow** aWindow) { return _to##GetWindow(aWindow); } \
  NS_IMETHOD    GetSelf(nsIDOMWindow** aSelf) { return _to##GetSelf(aSelf); } \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument) { return _to##GetDocument(aDocument); } \
  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator) { return _to##GetNavigator(aNavigator); } \
  NS_IMETHOD    GetParent(nsIDOMWindow** aParent) { return _to##GetParent(aParent); } \
  NS_IMETHOD    GetTop(nsIDOMWindow** aTop) { return _to##GetTop(aTop); } \
  NS_IMETHOD    GetClosed(PRBool* aClosed) { return _to##GetClosed(aClosed); } \
  NS_IMETHOD    GetFrames(nsIDOMWindowCollection** aFrames) { return _to##GetFrames(aFrames); } \
  NS_IMETHOD    GetOpener(nsIDOMWindow** aOpener) { return _to##GetOpener(aOpener); } \
  NS_IMETHOD    SetOpener(nsIDOMWindow* aOpener) { return _to##SetOpener(aOpener); } \
  NS_IMETHOD    GetStatus(nsString& aStatus) { return _to##GetStatus(aStatus); } \
  NS_IMETHOD    SetStatus(const nsString& aStatus) { return _to##SetStatus(aStatus); } \
  NS_IMETHOD    GetDefaultStatus(nsString& aDefaultStatus) { return _to##GetDefaultStatus(aDefaultStatus); } \
  NS_IMETHOD    SetDefaultStatus(const nsString& aDefaultStatus) { return _to##SetDefaultStatus(aDefaultStatus); } \
  NS_IMETHOD    GetName(nsString& aName) { return _to##GetName(aName); } \
  NS_IMETHOD    SetName(const nsString& aName) { return _to##SetName(aName); } \
  NS_IMETHOD    Dump(const nsString& aStr) { return _to##Dump(aStr); }  \
  NS_IMETHOD    Alert(const nsString& aStr) { return _to##Alert(aStr); }  \
  NS_IMETHOD    Focus() { return _to##Focus(); }  \
  NS_IMETHOD    Blur() { return _to##Blur(); }  \
  NS_IMETHOD    Close() { return _to##Close(); }  \
  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID) { return _to##ClearTimeout(aTimerID); }  \
  NS_IMETHOD    ClearInterval(PRInt32 aTimerID) { return _to##ClearInterval(aTimerID); }  \
  NS_IMETHOD    SetTimeout(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn) { return _to##SetTimeout(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    SetInterval(JSContext *cx, jsval *argv, PRUint32 argc, PRInt32* aReturn) { return _to##SetInterval(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Open(JSContext *cx, jsval *argv, PRUint32 argc, nsIDOMWindow** aReturn) { return _to##Open(cx, argv, argc, aReturn); }  \


extern nsresult NS_InitWindowClass(nsIScriptContext *aContext, nsIScriptGlobalObject *aGlobal);

extern "C" NS_DOM nsresult NS_NewScriptWindow(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMWindow_h__
