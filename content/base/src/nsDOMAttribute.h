/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsDOMAttribute_h___
#define nsDOMAttribute_h___

#include "nsIDOMAttr.h"
#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsGenericDOMNodeList.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsINodeInfo.h"

class nsIContent;
class nsDOMAttribute;

#define NS_IDOMATTRIBUTEPRIVATE_IID  \
 {0xa6cf90dd, 0x15b3, 0x11d2,        \
 {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIDOMAttributePrivate : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOMATTRIBUTEPRIVATE_IID)

  NS_IMETHOD DropReference() = 0;
  NS_IMETHOD SetContent(nsIContent* aContent) = 0;
  NS_IMETHOD GetContent(nsIContent** aContent) = 0;
};

// bogus child list for an attribute
class nsAttributeChildList : public nsGenericDOMNodeList
{
public:
  nsAttributeChildList(nsDOMAttribute* aAttribute);
  virtual ~nsAttributeChildList();

  // interface nsIDOMNodeList
  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);

  void DropReference();

protected:
  nsDOMAttribute* mAttribute;
};

// Attribute helper class used to wrap up an attribute with a dom
// object that implements nsIDOMAttr and nsIDOMNode
class nsDOMAttribute : public nsIDOMAttr,
                       public nsIDOM3Node,
                       public nsIDOMAttributePrivate
{
public:
  nsDOMAttribute(nsIContent* aContent, nsINodeInfo *aNodeInfo,
                 const nsAReadableString& aValue);
  virtual ~nsDOMAttribute();

  NS_DECL_ISUPPORTS

  // nsIDOMNode interface
  NS_DECL_NSIDOMNODE

  // nsIDOM3Node interface
  NS_DECL_NSIDOM3NODE

  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR
 
  // nsIDOMAttributePrivate interface
  NS_IMETHOD DropReference();
  NS_IMETHOD SetContent(nsIContent* aContent);
  NS_IMETHOD GetContent(nsIContent** aContent);

private:
  nsIContent* mContent;
  nsCOMPtr<nsINodeInfo> mNodeInfo;
  nsString mValue;
  // XXX For now, there's only a single child - a text
  // element representing the value
  nsIDOMText* mChild;
  nsAttributeChildList* mChildList;
};


#endif /* nsDOMAttribute_h___ */
