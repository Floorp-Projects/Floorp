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

  Private interface to the XBL Binding

*/

#ifndef nsIBinding_Manager_h__
#define nsIBinding_Manager_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsISupportsArray.h"

class nsIContent;
class nsIXBLBinding;
class nsIXBLDocumentInfo;
class nsIAtom;
class nsIStreamListener;

// {55D70FE0-C8E5-11d3-97FB-00400553EEF0}
#define NS_IBINDING_MANAGER_IID \
{ 0x55d70fe0, 0xc8e5, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIBindingManager : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IBINDING_MANAGER_IID; return iid; }

  NS_IMETHOD GetBinding(nsIContent* aContent, nsIXBLBinding** aResult) = 0;
  NS_IMETHOD SetBinding(nsIContent* aContent, nsIXBLBinding* aBinding) = 0;

  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  NS_IMETHOD GetInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult) = 0;
  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aParent, nsIContent** aResult, 
                                     PRBool* aMultipleInsertionPoints) = 0;

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL) = 0;
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL) = 0;
  NS_IMETHOD LoadBindingDocument(nsIDocument* aDocument, const nsAReadableString& aURL) = 0;

  NS_IMETHOD AddToAttachedQueue(nsIXBLBinding* aBinding)=0;
  NS_IMETHOD ClearAttachedQueue()=0;
  NS_IMETHOD ProcessAttachedQueue()=0;

  NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)=0;
  NS_IMETHOD GetXBLDocumentInfo(const nsCString& aURL, nsIXBLDocumentInfo** aResult)=0;

  NS_IMETHOD PutLoadingDocListener(const nsCString& aURL, nsIStreamListener* aListener) = 0;
  NS_IMETHOD GetLoadingDocListener(const nsCString& aURL, nsIStreamListener** aResult) = 0;
  NS_IMETHOD RemoveLoadingDocListener(const nsCString& aURL)=0;

  NS_IMETHOD InheritsStyle(nsIContent* aContent, PRBool* aResult) = 0;
  NS_IMETHOD FlushChromeBindings() = 0;
};

#endif // nsIBinding_Manager_h__
