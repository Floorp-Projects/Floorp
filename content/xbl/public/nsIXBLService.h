/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

/*

  Private interface to the XBL service

*/

#ifndef nsIXBLService_h__
#define nsIXBLService_h__

#include "nsString.h"
#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsIDOMEventReceiver;
class nsIDOMNodeList;
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
                          nsIXBLBinding** aBinding, PRBool* aResolveStyle) = 0;

  // This function clears out the bindings on a given content node.
  NS_IMETHOD FlushStyleBindings(nsIContent* aContent) = 0;

  // This method loads a binding doc and then builds the specific binding required.
  NS_IMETHOD GetBinding(nsIContent* aBoundElement, const nsCString& aURLStr, nsIXBLBinding** aResult) = 0;

  // Indicates whether or not a binding is fully loaded.
  NS_IMETHOD BindingReady(nsIContent* aBoundElement, const nsCString& aURLStr, PRBool* aIsReady) = 0;

  // Retrieves our base class (e.g., tells us what type of frame and content node to build)
  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  NS_IMETHOD GetXBLDocumentInfo(const nsCString& aURLStr, nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)=0;

  // This method checks the hashtable and then calls FetchBindingDocument on a miss.
  NS_IMETHOD LoadBindingDocumentInfo(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                     const nsCString& aURI, const nsCString& aRef,
                                     PRBool aForceSyncLoad, nsIXBLDocumentInfo** aResult) = 0;

  // Hooks up the global key and DragDrop event handlers to the document root.
  NS_IMETHOD AttachGlobalKeyHandler(nsIDOMEventReceiver* aElement)=0;
  NS_IMETHOD AttachGlobalDragHandler(nsIDOMEventReceiver* aElement)=0;
};

#endif // nsIXBLService_h__
