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

#ifndef nsIDOMStyleSheet_h__
#define nsIDOMStyleSheet_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;
class nsIDOMStyleSheet;
class nsIDOMMediaList;

#define NS_IDOMSTYLESHEET_IID \
 { 0xa6cf9080, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMStyleSheet : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSTYLESHEET_IID; return iid; }

  NS_IMETHOD    GetType(nsAWritableString& aType)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetOwnerNode(nsIDOMNode** aOwnerNode)=0;

  NS_IMETHOD    GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet)=0;

  NS_IMETHOD    GetHref(nsAWritableString& aHref)=0;

  NS_IMETHOD    GetTitle(nsAWritableString& aTitle)=0;

  NS_IMETHOD    GetMedia(nsIDOMMediaList** aMedia)=0;
};


#define NS_DECL_IDOMSTYLESHEET   \
  NS_IMETHOD    GetType(nsAWritableString& aType);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetOwnerNode(nsIDOMNode** aOwnerNode);  \
  NS_IMETHOD    GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet);  \
  NS_IMETHOD    GetHref(nsAWritableString& aHref);  \
  NS_IMETHOD    GetTitle(nsAWritableString& aTitle);  \
  NS_IMETHOD    GetMedia(nsIDOMMediaList** aMedia);  \



#define NS_FORWARD_IDOMSTYLESHEET(_to)  \
  NS_IMETHOD    GetType(nsAWritableString& aType) { return _to GetType(aType); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetOwnerNode(nsIDOMNode** aOwnerNode) { return _to GetOwnerNode(aOwnerNode); } \
  NS_IMETHOD    GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet) { return _to GetParentStyleSheet(aParentStyleSheet); } \
  NS_IMETHOD    GetHref(nsAWritableString& aHref) { return _to GetHref(aHref); } \
  NS_IMETHOD    GetTitle(nsAWritableString& aTitle) { return _to GetTitle(aTitle); } \
  NS_IMETHOD    GetMedia(nsIDOMMediaList** aMedia) { return _to GetMedia(aMedia); } \


extern "C" NS_DOM nsresult NS_InitStyleSheetClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptStyleSheet(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMStyleSheet_h__
