/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Private interface to the XBL service

*/

#ifndef nsIXBLService_h__
#define nsIXBLService_h__

#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsIDOMEventTarget;
class nsIDOMNodeList;
class nsXBLBinding;
class nsXBLDocumentInfo;
class nsIURI;
class nsIAtom;
class nsIPrincipal;

#define NS_IXBLSERVICE_IID      \
{ 0x8a25483c, 0x1ac6, 0x4796, { 0xa6, 0x12, 0x5a, 0xe0, 0x5c, 0x83, 0x65, 0x0b } }

class nsIXBLService : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXBLSERVICE_IID)

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.  aOriginPrincipal must not be null here.
  NS_IMETHOD LoadBindings(nsIContent* aContent, nsIURI* aURL,
                          nsIPrincipal* aOriginPrincipal, bool aAugmentFlag,
                          nsXBLBinding** aBinding, bool* aResolveStyle) = 0;

  // Indicates whether or not a binding is fully loaded.
  NS_IMETHOD BindingReady(nsIContent* aBoundElement, nsIURI* aURI, bool* aIsReady) = 0;

  // Retrieves our base class (e.g., tells us what type of frame and content node to build)
  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  // This method checks the hashtable and then calls FetchBindingDocument on a
  // miss.  aOriginPrincipal or aBoundDocument may be null to bypass security
  // checks.
  NS_IMETHOD LoadBindingDocumentInfo(nsIContent* aBoundElement,
                                     nsIDocument* aBoundDocument,
                                     nsIURI* aBindingURI,
                                     nsIPrincipal* aOriginPrincipal,
                                     bool aForceSyncLoad,
                                     nsXBLDocumentInfo** aResult) = 0;

  // Hooks up the global key event handlers to the document root.
  NS_IMETHOD AttachGlobalKeyHandler(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD DetachGlobalKeyHandler(nsIDOMEventTarget* aTarget) = 0;
  
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXBLService, NS_IXBLSERVICE_IID)

#endif // nsIXBLService_h__
