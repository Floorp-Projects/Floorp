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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMWindowInternal_h__
#define nsIDOMWindowInternal_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMWindow.h"
#include "jsapi.h"

class nsIDOMNavigator;
class nsIPrompt;
class nsIDOMBarProp;
class nsIDOMScreen;
class nsIDOMWindowInternal;
class nsIDOMHistory;
class nsIDOMEvent;
class nsISidebar;
class nsIDOMPkcs11;
class nsIDOMCrypto;
class nsIControllers;

#define NS_IDOMWINDOWINTERNAL_IID \
 { 0x9c911860, 0x7dd9, 0x11d4, \
  { 0x9a, 0x83, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74 } } 

class nsIDOMWindowInternal : public nsIDOMWindow {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMWINDOWINTERNAL_IID; return iid; }

  NS_IMETHOD    GetWindow(nsIDOMWindowInternal** aWindow)=0;

  NS_IMETHOD    GetSelf(nsIDOMWindowInternal** aSelf)=0;

  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator)=0;

  NS_IMETHOD    GetScreen(nsIDOMScreen** aScreen)=0;

  NS_IMETHOD    GetHistory(nsIDOMHistory** aHistory)=0;

  NS_IMETHOD    Get_content(nsIDOMWindowInternal** a_content)=0;

  NS_IMETHOD    GetSidebar(nsISidebar** aSidebar)=0;

  NS_IMETHOD    GetPrompter(nsIPrompt** aPrompter)=0;

  NS_IMETHOD    GetMenubar(nsIDOMBarProp** aMenubar)=0;

  NS_IMETHOD    GetToolbar(nsIDOMBarProp** aToolbar)=0;

  NS_IMETHOD    GetLocationbar(nsIDOMBarProp** aLocationbar)=0;

  NS_IMETHOD    GetPersonalbar(nsIDOMBarProp** aPersonalbar)=0;

  NS_IMETHOD    GetStatusbar(nsIDOMBarProp** aStatusbar)=0;

  NS_IMETHOD    GetDirectories(nsIDOMBarProp** aDirectories)=0;

  NS_IMETHOD    GetClosed(PRBool* aClosed)=0;

  NS_IMETHOD    GetCrypto(nsIDOMCrypto** aCrypto)=0;

  NS_IMETHOD    GetPkcs11(nsIDOMPkcs11** aPkcs11)=0;

  NS_IMETHOD    GetControllers(nsIControllers** aControllers)=0;

  NS_IMETHOD    GetOpener(nsIDOMWindowInternal** aOpener)=0;
  NS_IMETHOD    SetOpener(nsIDOMWindowInternal* aOpener)=0;

  NS_IMETHOD    GetStatus(nsAWritableString& aStatus)=0;
  NS_IMETHOD    SetStatus(const nsAReadableString& aStatus)=0;

  NS_IMETHOD    GetDefaultStatus(nsAWritableString& aDefaultStatus)=0;
  NS_IMETHOD    SetDefaultStatus(const nsAReadableString& aDefaultStatus)=0;

  NS_IMETHOD    GetLocation(jsval* aLocation)=0;
  NS_IMETHOD    SetLocation(jsval aLocation)=0;

  NS_IMETHOD    GetTitle(nsAWritableString& aTitle)=0;
  NS_IMETHOD    SetTitle(const nsAReadableString& aTitle)=0;

  NS_IMETHOD    GetInnerWidth(PRInt32* aInnerWidth)=0;
  NS_IMETHOD    SetInnerWidth(PRInt32 aInnerWidth)=0;

  NS_IMETHOD    GetInnerHeight(PRInt32* aInnerHeight)=0;
  NS_IMETHOD    SetInnerHeight(PRInt32 aInnerHeight)=0;

  NS_IMETHOD    GetOuterWidth(PRInt32* aOuterWidth)=0;
  NS_IMETHOD    SetOuterWidth(PRInt32 aOuterWidth)=0;

  NS_IMETHOD    GetOuterHeight(PRInt32* aOuterHeight)=0;
  NS_IMETHOD    SetOuterHeight(PRInt32 aOuterHeight)=0;

  NS_IMETHOD    GetScreenX(PRInt32* aScreenX)=0;
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX)=0;

  NS_IMETHOD    GetScreenY(PRInt32* aScreenY)=0;
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY)=0;

  NS_IMETHOD    GetPageXOffset(PRInt32* aPageXOffset)=0;
  NS_IMETHOD    SetPageXOffset(PRInt32 aPageXOffset)=0;

  NS_IMETHOD    GetPageYOffset(PRInt32* aPageYOffset)=0;
  NS_IMETHOD    SetPageYOffset(PRInt32 aPageYOffset)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    Dump(const nsAReadableString& aStr)=0;

  NS_IMETHOD    Alert(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    Confirm(JSContext* cx, jsval* argv, PRUint32 argc, PRBool* aReturn)=0;

  NS_IMETHOD    Prompt(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn)=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Back()=0;

  NS_IMETHOD    Forward()=0;

  NS_IMETHOD    Home()=0;

  NS_IMETHOD    Stop()=0;

  NS_IMETHOD    Print()=0;

  NS_IMETHOD    MoveTo(PRInt32 aXPos, PRInt32 aYPos)=0;

  NS_IMETHOD    MoveBy(PRInt32 aXDif, PRInt32 aYDif)=0;

  NS_IMETHOD    ResizeTo(PRInt32 aWidth, PRInt32 aHeight)=0;

  NS_IMETHOD    ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)=0;

  NS_IMETHOD    SizeToContent()=0;

  NS_IMETHOD    GetAttention()=0;

  NS_IMETHOD    Scroll(PRInt32 aXScroll, PRInt32 aYScroll)=0;

  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID)=0;

  NS_IMETHOD    ClearInterval(PRInt32 aTimerID)=0;

  NS_IMETHOD    SetTimeout(JSContext* cx, jsval* argv, PRUint32 argc, PRInt32* aReturn)=0;

  NS_IMETHOD    SetInterval(JSContext* cx, jsval* argv, PRUint32 argc, PRInt32* aReturn)=0;

  NS_IMETHOD    CaptureEvents(PRInt32 aEventFlags)=0;

  NS_IMETHOD    ReleaseEvents(PRInt32 aEventFlags)=0;

  NS_IMETHOD    RouteEvent(nsIDOMEvent* aEvt)=0;

  NS_IMETHOD    EnableExternalCapture()=0;

  NS_IMETHOD    DisableExternalCapture()=0;

  NS_IMETHOD    SetCursor(const nsAReadableString& aCursor)=0;

  NS_IMETHOD    Open(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMWindowInternal** aReturn)=0;

  NS_IMETHOD    OpenDialog(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMWindowInternal** aReturn)=0;

  NS_IMETHOD    Close()=0;

  NS_IMETHOD    Close(JSContext* cx, jsval* argv, PRUint32 argc)=0;

  NS_IMETHOD    UpdateCommands(const nsAReadableString& aAction)=0;

  NS_IMETHOD    Escape(const nsAReadableString& aStr, nsAWritableString& aReturn)=0;

  NS_IMETHOD    Unescape(const nsAReadableString& aStr, nsAWritableString& aReturn)=0;
};


#define NS_DECL_IDOMWINDOWINTERNAL   \
  NS_IMETHOD    GetWindow(nsIDOMWindowInternal** aWindow);  \
  NS_IMETHOD    GetSelf(nsIDOMWindowInternal** aSelf);  \
  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator);  \
  NS_IMETHOD    GetScreen(nsIDOMScreen** aScreen);  \
  NS_IMETHOD    GetHistory(nsIDOMHistory** aHistory);  \
  NS_IMETHOD    Get_content(nsIDOMWindowInternal** a_content);  \
  NS_IMETHOD    GetSidebar(nsISidebar** aSidebar);  \
  NS_IMETHOD    GetPrompter(nsIPrompt** aPrompter);  \
  NS_IMETHOD    GetMenubar(nsIDOMBarProp** aMenubar);  \
  NS_IMETHOD    GetToolbar(nsIDOMBarProp** aToolbar);  \
  NS_IMETHOD    GetLocationbar(nsIDOMBarProp** aLocationbar);  \
  NS_IMETHOD    GetPersonalbar(nsIDOMBarProp** aPersonalbar);  \
  NS_IMETHOD    GetStatusbar(nsIDOMBarProp** aStatusbar);  \
  NS_IMETHOD    GetDirectories(nsIDOMBarProp** aDirectories);  \
  NS_IMETHOD    GetClosed(PRBool* aClosed);  \
  NS_IMETHOD    GetCrypto(nsIDOMCrypto** aCrypto);  \
  NS_IMETHOD    GetPkcs11(nsIDOMPkcs11** aPkcs11);  \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers);  \
  NS_IMETHOD    GetOpener(nsIDOMWindowInternal** aOpener);  \
  NS_IMETHOD    SetOpener(nsIDOMWindowInternal* aOpener);  \
  NS_IMETHOD    GetStatus(nsAWritableString& aStatus);  \
  NS_IMETHOD    SetStatus(const nsAReadableString& aStatus);  \
  NS_IMETHOD    GetDefaultStatus(nsAWritableString& aDefaultStatus);  \
  NS_IMETHOD    SetDefaultStatus(const nsAReadableString& aDefaultStatus);  \
  NS_IMETHOD    GetLocation(jsval* aLocation);  \
  NS_IMETHOD    SetLocation(jsval aLocation);  \
  NS_IMETHOD    GetTitle(nsAWritableString& aTitle);  \
  NS_IMETHOD    SetTitle(const nsAReadableString& aTitle);  \
  NS_IMETHOD    GetInnerWidth(PRInt32* aInnerWidth);  \
  NS_IMETHOD    SetInnerWidth(PRInt32 aInnerWidth);  \
  NS_IMETHOD    GetInnerHeight(PRInt32* aInnerHeight);  \
  NS_IMETHOD    SetInnerHeight(PRInt32 aInnerHeight);  \
  NS_IMETHOD    GetOuterWidth(PRInt32* aOuterWidth);  \
  NS_IMETHOD    SetOuterWidth(PRInt32 aOuterWidth);  \
  NS_IMETHOD    GetOuterHeight(PRInt32* aOuterHeight);  \
  NS_IMETHOD    SetOuterHeight(PRInt32 aOuterHeight);  \
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX);  \
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX);  \
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY);  \
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY);  \
  NS_IMETHOD    GetPageXOffset(PRInt32* aPageXOffset);  \
  NS_IMETHOD    SetPageXOffset(PRInt32 aPageXOffset);  \
  NS_IMETHOD    GetPageYOffset(PRInt32* aPageYOffset);  \
  NS_IMETHOD    SetPageYOffset(PRInt32 aPageYOffset);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    Dump(const nsAReadableString& aStr);  \
  NS_IMETHOD    Alert(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    Confirm(JSContext* cx, jsval* argv, PRUint32 argc, PRBool* aReturn);  \
  NS_IMETHOD    Prompt(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn);  \
  NS_IMETHOD    Focus();  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Back();  \
  NS_IMETHOD    Forward();  \
  NS_IMETHOD    Home();  \
  NS_IMETHOD    Stop();  \
  NS_IMETHOD    Print();  \
  NS_IMETHOD    MoveTo(PRInt32 aXPos, PRInt32 aYPos);  \
  NS_IMETHOD    MoveBy(PRInt32 aXDif, PRInt32 aYDif);  \
  NS_IMETHOD    ResizeTo(PRInt32 aWidth, PRInt32 aHeight);  \
  NS_IMETHOD    ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif);  \
  NS_IMETHOD    SizeToContent();  \
  NS_IMETHOD    GetAttention();  \
  NS_IMETHOD    Scroll(PRInt32 aXScroll, PRInt32 aYScroll);  \
  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID);  \
  NS_IMETHOD    ClearInterval(PRInt32 aTimerID);  \
  NS_IMETHOD    SetTimeout(JSContext* cx, jsval* argv, PRUint32 argc, PRInt32* aReturn);  \
  NS_IMETHOD    SetInterval(JSContext* cx, jsval* argv, PRUint32 argc, PRInt32* aReturn);  \
  NS_IMETHOD    CaptureEvents(PRInt32 aEventFlags);  \
  NS_IMETHOD    ReleaseEvents(PRInt32 aEventFlags);  \
  NS_IMETHOD    RouteEvent(nsIDOMEvent* aEvt);  \
  NS_IMETHOD    EnableExternalCapture();  \
  NS_IMETHOD    DisableExternalCapture();  \
  NS_IMETHOD    SetCursor(const nsAReadableString& aCursor);  \
  NS_IMETHOD    Open(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMWindowInternal** aReturn);  \
  NS_IMETHOD    OpenDialog(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMWindowInternal** aReturn);  \
  NS_IMETHOD    Close();  \
  NS_IMETHOD    Close(JSContext* cx, jsval* argv, PRUint32 argc);  \
  NS_IMETHOD    UpdateCommands(const nsAReadableString& aAction);  \
  NS_IMETHOD    Escape(const nsAReadableString& aStr, nsAWritableString& aReturn);  \
  NS_IMETHOD    Unescape(const nsAReadableString& aStr, nsAWritableString& aReturn);  \



#define NS_FORWARD_IDOMWINDOWINTERNAL(_to)  \
  NS_IMETHOD    GetWindow(nsIDOMWindowInternal** aWindow) { return _to GetWindow(aWindow); } \
  NS_IMETHOD    GetSelf(nsIDOMWindowInternal** aSelf) { return _to GetSelf(aSelf); } \
  NS_IMETHOD    GetNavigator(nsIDOMNavigator** aNavigator) { return _to GetNavigator(aNavigator); } \
  NS_IMETHOD    GetScreen(nsIDOMScreen** aScreen) { return _to GetScreen(aScreen); } \
  NS_IMETHOD    GetHistory(nsIDOMHistory** aHistory) { return _to GetHistory(aHistory); } \
  NS_IMETHOD    Get_content(nsIDOMWindowInternal** a_content) { return _to Get_content(a_content); } \
  NS_IMETHOD    GetSidebar(nsISidebar** aSidebar) { return _to GetSidebar(aSidebar); } \
  NS_IMETHOD    GetPrompter(nsIPrompt** aPrompter) { return _to GetPrompter(aPrompter); } \
  NS_IMETHOD    GetMenubar(nsIDOMBarProp** aMenubar) { return _to GetMenubar(aMenubar); } \
  NS_IMETHOD    GetToolbar(nsIDOMBarProp** aToolbar) { return _to GetToolbar(aToolbar); } \
  NS_IMETHOD    GetLocationbar(nsIDOMBarProp** aLocationbar) { return _to GetLocationbar(aLocationbar); } \
  NS_IMETHOD    GetPersonalbar(nsIDOMBarProp** aPersonalbar) { return _to GetPersonalbar(aPersonalbar); } \
  NS_IMETHOD    GetStatusbar(nsIDOMBarProp** aStatusbar) { return _to GetStatusbar(aStatusbar); } \
  NS_IMETHOD    GetDirectories(nsIDOMBarProp** aDirectories) { return _to GetDirectories(aDirectories); } \
  NS_IMETHOD    GetClosed(PRBool* aClosed) { return _to GetClosed(aClosed); } \
  NS_IMETHOD    GetCrypto(nsIDOMCrypto** aCrypto) { return _to GetCrypto(aCrypto); } \
  NS_IMETHOD    GetPkcs11(nsIDOMPkcs11** aPkcs11) { return _to GetPkcs11(aPkcs11); } \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers) { return _to GetControllers(aControllers); } \
  NS_IMETHOD    GetOpener(nsIDOMWindowInternal** aOpener) { return _to GetOpener(aOpener); } \
  NS_IMETHOD    SetOpener(nsIDOMWindowInternal* aOpener) { return _to SetOpener(aOpener); } \
  NS_IMETHOD    GetStatus(nsAWritableString& aStatus) { return _to GetStatus(aStatus); } \
  NS_IMETHOD    SetStatus(const nsAReadableString& aStatus) { return _to SetStatus(aStatus); } \
  NS_IMETHOD    GetDefaultStatus(nsAWritableString& aDefaultStatus) { return _to GetDefaultStatus(aDefaultStatus); } \
  NS_IMETHOD    SetDefaultStatus(const nsAReadableString& aDefaultStatus) { return _to SetDefaultStatus(aDefaultStatus); } \
  NS_IMETHOD    GetLocation(jsval* aLocation) { return _to GetLocation(aLocation); } \
  NS_IMETHOD    SetLocation(jsval aLocation) { return _to SetLocation(aLocation); } \
  NS_IMETHOD    GetTitle(nsAWritableString& aTitle) { return _to GetTitle(aTitle); } \
  NS_IMETHOD    SetTitle(const nsAReadableString& aTitle) { return _to SetTitle(aTitle); } \
  NS_IMETHOD    GetInnerWidth(PRInt32* aInnerWidth) { return _to GetInnerWidth(aInnerWidth); } \
  NS_IMETHOD    SetInnerWidth(PRInt32 aInnerWidth) { return _to SetInnerWidth(aInnerWidth); } \
  NS_IMETHOD    GetInnerHeight(PRInt32* aInnerHeight) { return _to GetInnerHeight(aInnerHeight); } \
  NS_IMETHOD    SetInnerHeight(PRInt32 aInnerHeight) { return _to SetInnerHeight(aInnerHeight); } \
  NS_IMETHOD    GetOuterWidth(PRInt32* aOuterWidth) { return _to GetOuterWidth(aOuterWidth); } \
  NS_IMETHOD    SetOuterWidth(PRInt32 aOuterWidth) { return _to SetOuterWidth(aOuterWidth); } \
  NS_IMETHOD    GetOuterHeight(PRInt32* aOuterHeight) { return _to GetOuterHeight(aOuterHeight); } \
  NS_IMETHOD    SetOuterHeight(PRInt32 aOuterHeight) { return _to SetOuterHeight(aOuterHeight); } \
  NS_IMETHOD    GetScreenX(PRInt32* aScreenX) { return _to GetScreenX(aScreenX); } \
  NS_IMETHOD    SetScreenX(PRInt32 aScreenX) { return _to SetScreenX(aScreenX); } \
  NS_IMETHOD    GetScreenY(PRInt32* aScreenY) { return _to GetScreenY(aScreenY); } \
  NS_IMETHOD    SetScreenY(PRInt32 aScreenY) { return _to SetScreenY(aScreenY); } \
  NS_IMETHOD    GetPageXOffset(PRInt32* aPageXOffset) { return _to GetPageXOffset(aPageXOffset); } \
  NS_IMETHOD    SetPageXOffset(PRInt32 aPageXOffset) { return _to SetPageXOffset(aPageXOffset); } \
  NS_IMETHOD    GetPageYOffset(PRInt32* aPageYOffset) { return _to GetPageYOffset(aPageYOffset); } \
  NS_IMETHOD    SetPageYOffset(PRInt32 aPageYOffset) { return _to SetPageYOffset(aPageYOffset); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    Dump(const nsAReadableString& aStr) { return _to Dump(aStr); }  \
  NS_IMETHOD    Alert(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Alert(cx, argv, argc); }  \
  NS_IMETHOD    Confirm(JSContext* cx, jsval* argv, PRUint32 argc, PRBool* aReturn) { return _to Confirm(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Prompt(JSContext* cx, jsval* argv, PRUint32 argc, jsval* aReturn) { return _to Prompt(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Back() { return _to Back(); }  \
  NS_IMETHOD    Forward() { return _to Forward(); }  \
  NS_IMETHOD    Home() { return _to Home(); }  \
  NS_IMETHOD    Stop() { return _to Stop(); }  \
  NS_IMETHOD    Print() { return _to Print(); }  \
  NS_IMETHOD    MoveTo(PRInt32 aXPos, PRInt32 aYPos) { return _to MoveTo(aXPos, aYPos); }  \
  NS_IMETHOD    MoveBy(PRInt32 aXDif, PRInt32 aYDif) { return _to MoveBy(aXDif, aYDif); }  \
  NS_IMETHOD    ResizeTo(PRInt32 aWidth, PRInt32 aHeight) { return _to ResizeTo(aWidth, aHeight); }  \
  NS_IMETHOD    ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif) { return _to ResizeBy(aWidthDif, aHeightDif); }  \
  NS_IMETHOD    SizeToContent() { return _to SizeToContent(); }  \
  NS_IMETHOD    GetAttention() { return _to GetAttention(); }  \
  NS_IMETHOD    Scroll(PRInt32 aXScroll, PRInt32 aYScroll) { return _to Scroll(aXScroll, aYScroll); }  \
  NS_IMETHOD    ClearTimeout(PRInt32 aTimerID) { return _to ClearTimeout(aTimerID); }  \
  NS_IMETHOD    ClearInterval(PRInt32 aTimerID) { return _to ClearInterval(aTimerID); }  \
  NS_IMETHOD    SetTimeout(JSContext* cx, jsval* argv, PRUint32 argc, PRInt32* aReturn) { return _to SetTimeout(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    SetInterval(JSContext* cx, jsval* argv, PRUint32 argc, PRInt32* aReturn) { return _to SetInterval(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    CaptureEvents(PRInt32 aEventFlags) { return _to CaptureEvents(aEventFlags); }  \
  NS_IMETHOD    ReleaseEvents(PRInt32 aEventFlags) { return _to ReleaseEvents(aEventFlags); }  \
  NS_IMETHOD    RouteEvent(nsIDOMEvent* aEvt) { return _to RouteEvent(aEvt); }  \
  NS_IMETHOD    EnableExternalCapture() { return _to EnableExternalCapture(); }  \
  NS_IMETHOD    DisableExternalCapture() { return _to DisableExternalCapture(); }  \
  NS_IMETHOD    SetCursor(const nsAReadableString& aCursor) { return _to SetCursor(aCursor); }  \
  NS_IMETHOD    Open(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMWindowInternal** aReturn) { return _to Open(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    OpenDialog(JSContext* cx, jsval* argv, PRUint32 argc, nsIDOMWindowInternal** aReturn) { return _to OpenDialog(cx, argv, argc, aReturn); }  \
  NS_IMETHOD    Close() { return _to Close(); }  \
  NS_IMETHOD    Close(JSContext* cx, jsval* argv, PRUint32 argc) { return _to Close(cx, argv, argc); }  \
  NS_IMETHOD    UpdateCommands(const nsAReadableString& aAction) { return _to UpdateCommands(aAction); }  \
  NS_IMETHOD    Escape(const nsAReadableString& aStr, nsAWritableString& aReturn) { return _to Escape(aStr, aReturn); }  \
  NS_IMETHOD    Unescape(const nsAReadableString& aStr, nsAWritableString& aReturn) { return _to Unescape(aStr, aReturn); }  \


#endif // nsIDOMWindowInternal_h__
