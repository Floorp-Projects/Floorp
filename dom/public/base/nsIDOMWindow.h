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

#ifndef nsIDOMWindow_h__
#define nsIDOMWindow_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMDocument;
class nsIDOMBarProp;
class nsIDOMWindowCollection;
class nsISelection;
class nsIDOMWindow;

#define NS_IDOMWINDOW_IID \
 { 0xa6cf906b, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMWindow : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMWINDOW_IID; return iid; }

  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument)=0;

  NS_IMETHOD    GetParent(nsIDOMWindow** aParent)=0;

  NS_IMETHOD    GetTop(nsIDOMWindow** aTop)=0;

  NS_IMETHOD    GetScrollbars(nsIDOMBarProp** aScrollbars)=0;

  NS_IMETHOD    GetFrames(nsIDOMWindowCollection** aFrames)=0;

  NS_IMETHOD    GetName(nsAWritableString& aName)=0;
  NS_IMETHOD    SetName(const nsAReadableString& aName)=0;

  NS_IMETHOD    GetScrollX(PRInt32* aScrollX)=0;

  NS_IMETHOD    GetScrollY(PRInt32* aScrollY)=0;

  NS_IMETHOD    ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)=0;

  NS_IMETHOD    ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif)=0;

  NS_IMETHOD    GetSelection(nsISelection** aReturn)=0;

  NS_IMETHOD    ScrollByLines(PRInt32 aNumLines)=0;

  NS_IMETHOD    ScrollByPages(PRInt32 aNumPages)=0;
};


#define NS_DECL_IDOMWINDOW   \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument);  \
  NS_IMETHOD    GetParent(nsIDOMWindow** aParent);  \
  NS_IMETHOD    GetTop(nsIDOMWindow** aTop);  \
  NS_IMETHOD    GetScrollbars(nsIDOMBarProp** aScrollbars);  \
  NS_IMETHOD    GetFrames(nsIDOMWindowCollection** aFrames);  \
  NS_IMETHOD    GetName(nsAWritableString& aName);  \
  NS_IMETHOD    SetName(const nsAReadableString& aName);  \
  NS_IMETHOD    GetScrollX(PRInt32* aScrollX);  \
  NS_IMETHOD    GetScrollY(PRInt32* aScrollY);  \
  NS_IMETHOD    ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll);  \
  NS_IMETHOD    ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif);  \
  NS_IMETHOD    GetSelection(nsISelection** aReturn);  \
  NS_IMETHOD    ScrollByLines(PRInt32 aNumLines);  \
  NS_IMETHOD    ScrollByPages(PRInt32 aNumPages);  \



#define NS_FORWARD_IDOMWINDOW(_to)  \
  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument) { return _to GetDocument(aDocument); } \
  NS_IMETHOD    GetParent(nsIDOMWindow** aParent) { return _to GetParent(aParent); } \
  NS_IMETHOD    GetTop(nsIDOMWindow** aTop) { return _to GetTop(aTop); } \
  NS_IMETHOD    GetScrollbars(nsIDOMBarProp** aScrollbars) { return _to GetScrollbars(aScrollbars); } \
  NS_IMETHOD    GetFrames(nsIDOMWindowCollection** aFrames) { return _to GetFrames(aFrames); } \
  NS_IMETHOD    GetName(nsAWritableString& aName) { return _to GetName(aName); } \
  NS_IMETHOD    SetName(const nsAReadableString& aName) { return _to SetName(aName); } \
  NS_IMETHOD    GetScrollX(PRInt32* aScrollX) { return _to GetScrollX(aScrollX); } \
  NS_IMETHOD    GetScrollY(PRInt32* aScrollY) { return _to GetScrollY(aScrollY); } \
  NS_IMETHOD    ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll) { return _to ScrollTo(aXScroll, aYScroll); }  \
  NS_IMETHOD    ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif) { return _to ScrollBy(aXScrollDif, aYScrollDif); }  \
  NS_IMETHOD    GetSelection(nsISelection** aReturn) { return _to GetSelection(aReturn); }  \
  NS_IMETHOD    ScrollByLines(PRInt32 aNumLines) { return _to ScrollByLines(aNumLines); }  \
  NS_IMETHOD    ScrollByPages(PRInt32 aNumPages) { return _to ScrollByPages(aNumPages); }  \


extern nsresult NS_InitWindowClass(nsIScriptContext *aContext, nsIScriptGlobalObject *aGlobal);

extern "C" NS_DOM nsresult NS_NewScriptWindow(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMWindow_h__
