/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

/*

  Private interface to the XBL service

*/

#ifndef nsIXBLService_h__
#define nsIXBLService_h__

#include "nsString.h"
#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsISupportsArray;
class nsIXBLBinding;
class nsIXBLDocumentInfo;

// {0E7903E1-C7BB-11d3-97FB-00400553EEF0}
#define NS_IXBLSERVICE_IID \
{ 0xe7903e1, 0xc7bb, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIXBLService : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLSERVICE_IID; return iid; }

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.
  NS_IMETHOD LoadBindings(nsIContent* aContent, const nsAReadableString& aURL, PRBool aAugmentFlag,
                          nsIXBLBinding** aBinding) = 0;

  // This function clears out the bindings on a given content node.
  NS_IMETHOD FlushStyleBindings(nsIContent* aContent) = 0;

  // This method loads a binding doc and then builds the specific binding required.
  NS_IMETHOD GetBinding(nsIContent* aBoundElement, const nsCString& aURLStr, nsIXBLBinding** aResult) = 0;

  // Indicates whether or not a binding is fully loaded.
  NS_IMETHOD BindingReady(nsIContent* aBoundElement, const nsCString& aURLStr, PRBool* aIsReady) = 0;

  // For a given element, returns a flat list of all the anonymous children that need
  // frames built.
  NS_IMETHOD GetContentList(nsIContent* aContent, nsISupportsArray** aResult, nsIContent** aChildElement, 
                            PRBool* aMultipleInsertionPoints) = 0;

  // Retrieves our base class (e.g., tells us what type of frame and content node to build)
  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  NS_IMETHOD GetXBLDocumentInfo(const nsCString& aURLStr, nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)=0;

  // This method checks the hashtable and then calls FetchBindingDocument on a miss.
  NS_IMETHOD LoadBindingDocumentInfo(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                     const nsCString& aURI, const nsCString& aRef,
                                     PRBool aForceSyncLoad, nsIXBLDocumentInfo** aResult) = 0;

};

#endif // nsIXBLService_h__
