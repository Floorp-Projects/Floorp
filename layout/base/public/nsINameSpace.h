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

#ifndef nsINameSpace_h___
#define nsINameSpace_h___

#include "nsISupports.h"
#include "nslayout.h"

class nsIAtom;
class nsString;
class nsINameSpaceManager;

#define NS_INAMESPACE_IID \
  { 0xa6cf90d4, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}


/**
 * A nsINameSpace registers the NameSpace URI with the NameSpaceManager
 * (creating or finding an ID), and manages the relationship between
 * the NameSpace ID and the (optional) Prefix.
 *
 * New NameSpaces are created as a child of an existing NameSpace. Searches
 * for NameSpaces based on prefix search up the chain of nested NameSpaces
 *
 * Each NameSpace keeps a live reference on its parent and its Manager.
 *
 */
class nsINameSpace : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_INAMESPACE_IID; return iid; }

  NS_IMETHOD GetNameSpaceManager(nsINameSpaceManager*& aManager) const = 0;

  // Get data of this name space
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const = 0;
  NS_IMETHOD GetNameSpaceURI(nsAWritableString& aURI) const = 0;
  NS_IMETHOD GetNameSpacePrefix(nsIAtom*& aPrefix) const = 0;

  NS_IMETHOD GetParentNameSpace(nsINameSpace*& aParent) const = 0;

  // find name space within self and parents (not children)
  NS_IMETHOD FindNameSpace(nsIAtom* aPrefix, nsINameSpace*& aNameSpace) const = 0;
  NS_IMETHOD FindNameSpaceID(nsIAtom* aPrefix, PRInt32& aNameSpaceID) const = 0;
  NS_IMETHOD FindNameSpacePrefix(PRInt32 aNameSpaceID, nsIAtom*& aPrefix) const = 0;

  // create new child name space
  NS_IMETHOD CreateChildNameSpace(nsIAtom* aPrefix, 
                                  const nsAReadableString& aURI,
                                  nsINameSpace*& aChildNameSpace) = 0;

  NS_IMETHOD CreateChildNameSpace(nsIAtom* aPrefix, PRInt32 aNameSpaceID,
                                  nsINameSpace*& aChildNameSpace) = 0;
};

#endif // nsINameSpace_h___
