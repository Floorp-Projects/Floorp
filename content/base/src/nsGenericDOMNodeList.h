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

#ifndef nsGenericDOMNodeList_h__
#define nsGenericDOMNodeList_h__

#include "nsISupports.h"
#include "nsIDOMNodeList.h"

/**
 * This is a base class for a generic DOM Node List. The base class
 * provides implementations for nsISupports, it is up to the subclass
 * to implement the core node list methods:
 *
 *   GetLength
 *   Item
 * */
class nsGenericDOMNodeList : public nsIDOMNodeList 
{
public:
  nsGenericDOMNodeList();
  virtual ~nsGenericDOMNodeList();

  NS_DECL_ISUPPORTS

  // The following need to be defined in the subclass
  // nsIDOMNodeList interface
  NS_IMETHOD    GetLength(PRUint32* aLength)=0;
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn)=0;
};

#endif // nsGenericDOMNodeList_h__
