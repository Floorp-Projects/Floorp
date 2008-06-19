/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  Private interface to the XBL service

*/

#ifndef nsIXBLService_h__
#define nsIXBLService_h__

#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsPIDOMEventTarget;
class nsIDOMNodeList;
class nsXBLBinding;
class nsIXBLDocumentInfo;
class nsIURI;
class nsIAtom;
class nsIPrincipal;

#define NS_IXBLSERVICE_IID      \
{ 0x8d3b37f5, 0xde7e, 0x4595,   \
 { 0xb8, 0x56, 0xf7, 0x11, 0xe8, 0xe7, 0xb5, 0x59 } }

class nsIXBLService : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXBLSERVICE_IID)

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.  aOriginPrincipal must not be null here.
  NS_IMETHOD LoadBindings(nsIContent* aContent, nsIURI* aURL,
                          nsIPrincipal* aOriginPrincipal, PRBool aAugmentFlag,
                          nsXBLBinding** aBinding, PRBool* aResolveStyle) = 0;

  // Indicates whether or not a binding is fully loaded.
  NS_IMETHOD BindingReady(nsIContent* aBoundElement, nsIURI* aURI, PRBool* aIsReady) = 0;

  // Retrieves our base class (e.g., tells us what type of frame and content node to build)
  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  // This method checks the hashtable and then calls FetchBindingDocument on a
  // miss.  aOriginPrincipal or aBoundDocument may be null to bypass security
  // checks.
  NS_IMETHOD LoadBindingDocumentInfo(nsIContent* aBoundElement,
                                     nsIDocument* aBoundDocument,
                                     nsIURI* aBindingURI,
                                     nsIPrincipal* aOriginPrincipal,
                                     PRBool aForceSyncLoad,
                                     nsIXBLDocumentInfo** aResult) = 0;

  // Hooks up the global key event handlers to the document root.
  NS_IMETHOD AttachGlobalKeyHandler(nsPIDOMEventTarget* aTarget)=0;
  NS_IMETHOD DetachGlobalKeyHandler(nsPIDOMEventTarget* aTarget)=0;
  
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXBLService, NS_IXBLSERVICE_IID)

#endif // nsIXBLService_h__
