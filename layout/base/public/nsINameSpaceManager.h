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

#ifndef nsINameSpaceManager_h___
#define nsINameSpaceManager_h___

#include "nsISupports.h"
#include "nslayout.h"
#include "nsAWritableString.h"

class nsIAtom;
class nsString;
class nsINameSpace;

#define kNameSpaceID_Unknown -1
#define kNameSpaceID_None     0
#define kNameSpaceID_XMLNS    1 // not really a namespace, but it needs to play the game
#define kNameSpaceID_XML      2
#define kNameSpaceID_HTML     3
#define kNameSpaceID_XLink    4

// 'html' is by definition bound to the namespace name "urn:w3-org-ns:HTML" XXX ???
// 'xml' is by definition bound to the namespace name "urn:Connolly:input:required" XXX

#define NS_INAMESPACEMANAGER_IID \
  { 0xa6cf90d5, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * The Name Space Manager tracks the associtation between a NameSpace
 * URI and the PRInt32 runtime id. Mappings between NameSpaces and 
 * NameSpace prefixes are managed by nsINameSpaces
 *
 * All NameSpace URIs are stored in a global table so that IDs are
 * consistent accross the app. NameSpace IDs are only consistent at runtime
 * ie: they are not guaranteed to be consistent accross app sessions.
 *
 * The nsINameSpaceManager needs to have a live reference for as long as
 * the NameSpace IDs are needed. Generally, a document keeps a reference to 
 * a nsINameSpaceManager. Also, each nsINameSpace that comes from the manager
 * keeps a reference to it.
 *
 * To create a stack of NameSpaces, call CreateRootNameSpace, and then create
 * child NameSpaces from the root.
 *
 * The "html" and "xml" namespaces come "pre-canned" from the root.
 *
 */
class nsINameSpaceManager : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_INAMESPACEMANAGER_IID; return iid; }

  NS_IMETHOD CreateRootNameSpace(nsINameSpace*& aRootNameSpace) = 0;

  NS_IMETHOD RegisterNameSpace(const nsAReadableString& aURI, 
			                         PRInt32& aNameSpaceID) = 0;

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceID, nsAWritableString& aURI) = 0;
  NS_IMETHOD GetNameSpaceID(const nsAReadableString& aURI, PRInt32& aNameSpaceID) = 0;
};

extern NS_LAYOUT nsresult
  NS_NewNameSpaceManager(nsINameSpaceManager** aInstancePtrResult);


#endif // nsINameSpaceManager_h___
