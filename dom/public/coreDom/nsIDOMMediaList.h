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

#ifndef nsIDOMMediaList_h__
#define nsIDOMMediaList_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMMEDIALIST_IID \
 { 0x9b0c2ed7, 0x111c, 0x4824, \
  { 0xad, 0xf9, 0xef, 0x0d, 0xa6, 0xda, 0xd3, 0x71 } } 

class nsIDOMMediaList : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMMEDIALIST_IID; return iid; }

  NS_IMETHOD    GetMediaText(nsString& aMediaText)=0;
  NS_IMETHOD    SetMediaText(const nsString& aMediaText)=0;

  NS_IMETHOD    GetLength(PRUint32* aLength)=0;

  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn)=0;

  NS_IMETHOD    Delete(const nsString& aOldMedium)=0;

  NS_IMETHOD    Append(const nsString& aNewMedium)=0;
};


#define NS_DECL_IDOMMEDIALIST   \
  NS_IMETHOD    GetMediaText(nsString& aMediaText);  \
  NS_IMETHOD    SetMediaText(const nsString& aMediaText);  \
  NS_IMETHOD    GetLength(PRUint32* aLength);  \
  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn);  \
  NS_IMETHOD    Delete(const nsString& aOldMedium);  \
  NS_IMETHOD    Append(const nsString& aNewMedium);  \



#define NS_FORWARD_IDOMMEDIALIST(_to)  \
  NS_IMETHOD    GetMediaText(nsString& aMediaText) { return _to GetMediaText(aMediaText); } \
  NS_IMETHOD    SetMediaText(const nsString& aMediaText) { return _to SetMediaText(aMediaText); } \
  NS_IMETHOD    GetLength(PRUint32* aLength) { return _to GetLength(aLength); } \
  NS_IMETHOD    Item(PRUint32 aIndex, nsString& aReturn) { return _to Item(aIndex, aReturn); }  \
  NS_IMETHOD    Delete(const nsString& aOldMedium) { return _to Delete(aOldMedium); }  \
  NS_IMETHOD    Append(const nsString& aNewMedium) { return _to Append(aNewMedium); }  \


extern "C" NS_DOM nsresult NS_InitMediaListClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptMediaList(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMMediaList_h__
