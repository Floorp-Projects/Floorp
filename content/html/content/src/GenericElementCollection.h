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

#ifndef GenericElementCollection_h__
#define GenericElementCollection_h__

#include "nsGenericDOMHTMLCollection.h"

class nsIContent;
class nsIAtom;

/**
 * This class provides a late-bound collection of elements that are
 * direct decendents of an element.
 * mParent is NOT ref-counted to avoid circular references
 */
class GenericElementCollection : public nsGenericDOMHTMLCollection 
{
public:
  GenericElementCollection(nsIContent *aParent, 
                         nsIAtom *aTag);
  virtual ~GenericElementCollection();

  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD    NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);

  NS_IMETHOD    ParentDestroyed();

#ifdef DEBUG
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  nsIContent * mParent;
  nsIAtom * mTag;
};

#endif
