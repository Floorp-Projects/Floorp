/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

  Private interface to the XBL Binding

*/

#ifndef nsIBinding_Manager_h__
#define nsIBinding_Manager_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsISupportsArray.h"

class nsIContent;
class nsIXBLBinding;
class nsIXBLBindingAttachedHandler;
class nsIXBLDocumentInfo;
class nsIAtom;
class nsIStreamListener;
class nsIXPConnectWrappedJS;
class nsIDOMNodeList;

// {55D70FE0-C8E5-11d3-97FB-00400553EEF0}
#define NS_IBINDING_MANAGER_IID \
{ 0x55d70fe0, 0xc8e5, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIBindingManager : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IBINDING_MANAGER_IID; return iid; }

  NS_IMETHOD GetBinding(nsIContent* aContent, nsIXBLBinding** aResult) = 0;
  NS_IMETHOD SetBinding(nsIContent* aContent, nsIXBLBinding* aBinding) = 0;

  NS_IMETHOD GetInsertionParent(nsIContent* aContent, nsIContent** aResult)=0;
  NS_IMETHOD SetInsertionParent(nsIContent* aContent, nsIContent* aResult)=0;

  NS_IMETHOD GetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS** aResult) = 0;
  NS_IMETHOD SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aResult) = 0;

  /**
   * Notify the binding manager that an element
   * has been moved from one document to another,
   * so that it can update any bindings or
   * nsIAnonymousContentCreator-created anonymous
   * content that may depend on the document.
   * @param aContent the element that's being moved
   * @param aOldDocument the old document in which the
   *   content resided. May be null if the the content
   *   was not in any document.
   * @param aNewDocument the document in which the
   *   content will reside. May be null if the content
   *   will not reside in any document, or if the
   *   content is being destroyed.
   */
  NS_IMETHOD ChangeDocumentFor(nsIContent* aContent, nsIDocument* aOldDocument,
                               nsIDocument* aNewDocument) = 0;


  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  /**
   * Return a list of all explicit children, including any children
   * that may have been inserted via XBL insertion points.
   */
  NS_IMETHOD GetContentListFor(nsIContent* aContent, nsIDOMNodeList** aResult) = 0;

  /**
   * Set the insertion point children for the specified element.
   */
  NS_IMETHOD SetContentListFor(nsIContent* aContent, nsISupportsArray* aList)=0;

  /**
   * Determine whether or not the explicit child list has been altered
   * by XBL insertion points.
   */
  NS_IMETHOD HasContentListFor(nsIContent* aContent, PRBool* aResult) = 0;

  /**
   * For a given element, retrieve the anonymous child content.
   */
  NS_IMETHOD GetAnonymousNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult) = 0;

  /**
   * Set the anonymous child content for the specified element.
   */
  NS_IMETHOD SetAnonymousNodesFor(nsIContent* aContent, nsISupportsArray* aList) = 0;

  /**
   * Retrieves the anonymous list of children if the element has one;
   * otherwise, retrieves the list of explicit children. N.B. that if
   * the explicit child list has not been altered by XBL insertion
   * points, then aResult will be null.
   */
  NS_IMETHOD GetXBLChildNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult) = 0;

  /**
   * Given a parent element and a child content, determine where the
   * child content should be inserted in the parent element's
   * anonymous content tree. Specifically, aChild should be inserted
   * beneath aResult at the index specified by aIndex.
   */
  NS_IMETHOD GetInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex) = 0;

  /**
   * Return the unfiltered insertion point for the specified parent
   * element. If other filtered insertion points exist,
   * aMultipleInsertionPoints will be set to true.
   */
  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aParent, nsIContent** aResult, PRUint32* aIndex,  
                                     PRBool* aMultipleInsertionPoints) = 0;

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL) = 0;
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL) = 0;
  NS_IMETHOD LoadBindingDocument(nsIDocument* aDocument, const nsAReadableString& aURL,
                                 nsIDocument** aResult) = 0;

  NS_IMETHOD AddToAttachedQueue(nsIXBLBinding* aBinding)=0;
  NS_IMETHOD AddHandlerToAttachedQueue(nsIXBLBindingAttachedHandler* aHandler)=0;
  NS_IMETHOD ClearAttachedQueue()=0;
  NS_IMETHOD ProcessAttachedQueue()=0;

  NS_IMETHOD ExecuteDetachedHandlers()=0;

  NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)=0;
  NS_IMETHOD GetXBLDocumentInfo(const nsCString& aURL, nsIXBLDocumentInfo** aResult)=0;

  NS_IMETHOD PutLoadingDocListener(const nsCString& aURL, nsIStreamListener* aListener) = 0;
  NS_IMETHOD GetLoadingDocListener(const nsCString& aURL, nsIStreamListener** aResult) = 0;
  NS_IMETHOD RemoveLoadingDocListener(const nsCString& aURL)=0;

  NS_IMETHOD InheritsStyle(nsIContent* aContent, PRBool* aResult) = 0;
  NS_IMETHOD FlushChromeBindings() = 0;

  NS_IMETHOD GetBindingImplementation(nsIContent* aContent, REFNSIID aIID, void** aResult)=0;

  NS_IMETHOD ShouldBuildChildFrames(nsIContent* aContent, PRBool* aResult) = 0;
};

#endif // nsIBinding_Manager_h__
