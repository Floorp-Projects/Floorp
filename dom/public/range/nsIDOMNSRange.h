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

#ifndef nsIDOMNSRange_h__
#define nsIDOMNSRange_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;
class nsIDOMDocumentFragment;

#define NS_IDOMNSRANGE_IID \
 { 0xa6cf90f2, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMNSRange : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNSRANGE_IID; return iid; }
  enum {
    NODE_BEFORE = 0,
    NODE_AFTER = 1,
    NODE_BEFORE_AND_AFTER = 2,
    NODE_INSIDE = 3
  };

  NS_IMETHOD    CreateContextualFragment(const nsAReadableString& aFragment, nsIDOMDocumentFragment** aReturn)=0;

  NS_IMETHOD    IsValidFragment(const nsAReadableString& aFragment, PRBool* aReturn)=0;

  NS_IMETHOD    IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aReturn)=0;

  NS_IMETHOD    ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset, PRInt16* aReturn)=0;

  NS_IMETHOD    IntersectsNode(nsIDOMNode* aN, PRBool* aReturn)=0;

  NS_IMETHOD    CompareNode(nsIDOMNode* aN, PRUint16* aReturn)=0;
};


#define NS_DECL_IDOMNSRANGE   \
  NS_IMETHOD    CreateContextualFragment(const nsAReadableString& aFragment, nsIDOMDocumentFragment** aReturn);  \
  NS_IMETHOD    IsValidFragment(const nsAReadableString& aFragment, PRBool* aReturn);  \
  NS_IMETHOD    IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aReturn);  \
  NS_IMETHOD    ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset, PRInt16* aReturn);  \
  NS_IMETHOD    IntersectsNode(nsIDOMNode* aN, PRBool* aReturn);  \
  NS_IMETHOD    CompareNode(nsIDOMNode* aN, PRUint16* aReturn);  \



#define NS_FORWARD_IDOMNSRANGE(_to)  \
  NS_IMETHOD    CreateContextualFragment(const nsAReadableString& aFragment, nsIDOMDocumentFragment** aReturn) { return _to CreateContextualFragment(aFragment, aReturn); }  \
  NS_IMETHOD    IsValidFragment(const nsAReadableString& aFragment, PRBool* aReturn) { return _to IsValidFragment(aFragment, aReturn); }  \
  NS_IMETHOD    IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aReturn) { return _to IsPointInRange(aParent, aOffset, aReturn); }  \
  NS_IMETHOD    ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset, PRInt16* aReturn) { return _to ComparePoint(aParent, aOffset, aReturn); }  \
  NS_IMETHOD    IntersectsNode(nsIDOMNode* aN, PRBool* aReturn) { return _to IntersectsNode(aN, aReturn); }  \
  NS_IMETHOD    CompareNode(nsIDOMNode* aN, PRUint16* aReturn) { return _to CompareNode(aN, aReturn); }  \


#endif // nsIDOMNSRange_h__
