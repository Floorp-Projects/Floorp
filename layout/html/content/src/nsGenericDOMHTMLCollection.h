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

#ifndef nsGenericDOMHTMLCollection_h__
#define nsGenericDOMHTMLCollection_h__

#include "nsISupports.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptObjectOwner.h"

/**
 * This is a base class for a generic HTML collection. The base class
 * provides implementations for nsISupports and nsIScriptObjectOwner,
 * but it is up to the subclass to implement the core HTML collection
 * methods:
 *   GetLength
 *   Item
 *   NamedItem
 *
 */
class nsGenericDOMHTMLCollection : public nsIDOMHTMLCollection, 
                                   public nsIScriptObjectOwner 
{
public:
  nsGenericDOMHTMLCollection();
  virtual ~nsGenericDOMHTMLCollection();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // The following need to be defined in the subclass
  // nsIDOMHTMLCollection interface
  NS_IMETHOD    GetLength(PRUint32* aLength)=0;
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn)=0;
  NS_IMETHOD    NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)=0;

protected:
  void*        mScriptObject;
};

#endif // nsGenericDOMHTMLCollection_h__
