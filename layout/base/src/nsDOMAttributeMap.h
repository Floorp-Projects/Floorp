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
 * Contributor(s): 
 */
#ifndef nsDOMAttributeMap_h___
#define nsDOMAttributeMap_h___

#include "nsIDOMNamedNodeMap.h"
#include "nsIScriptObjectOwner.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "plhash.h"

class nsIContent;

// Helper class that implements the nsIDOMNamedNodeMap interface.
class nsDOMAttributeMap : public nsIDOMNamedNodeMap,
                          public nsIScriptObjectOwner
{
public:
  nsDOMAttributeMap(nsIContent* aContent);
  virtual ~nsDOMAttributeMap();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMNamedNodeMap interface
  NS_IMETHOD GetLength(PRUint32* aSize);
  NS_IMETHOD GetNamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD SetNamedItem(nsIDOMNode* aNode, nsIDOMNode** aReturn);
  NS_IMETHOD RemoveNamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD GetNamedItemNS(const nsAReadableString& aNamespaceURI,
                            const nsAReadableString& aLocalName, nsIDOMNode** aReturn);
  NS_IMETHOD SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn);
  NS_IMETHOD RemoveNamedItemNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aLocalName, 
                               nsIDOMNode** aReturn);

  void DropReference();

#ifdef DEBUG
  static nsresult SizeOfNamedNodeMap(nsIDOMNamedNodeMap* aMap,
                                     nsISizeOfHandler* aSizer,
                                     PRUint32* aResult);
#endif

private:
  nsIContent* mContent;
  void* mScriptObject;
};


#endif /* nsDOMAttributeMap_h___ */
