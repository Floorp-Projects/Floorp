/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/**
 * Private interface to the XBL Binding.  This interface is currently
 * undergoing deCOMtamination and some methods are only accessible from
 * inside of gklayout.
*/

#ifndef nsIBinding_Manager_h__
#define nsIBinding_Manager_h__

#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsXBLBinding;
class nsIXBLDocumentInfo;
class nsIAtom;
class nsIStreamListener;
class nsIURI;
class nsIXPConnectWrappedJS;
class nsIDOMNodeList;
class nsVoidArray;

#define NS_IBINDING_MANAGER_IID \
{ 0x92281eaa, 0x89c4, 0x4457, { 0x8f, 0x8d, 0xca, 0x92, 0xbf, 0xbe, 0x0f, 0x50 } }

class nsIBindingManager : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBINDING_MANAGER_IID)

  virtual nsXBLBinding* GetBinding(nsIContent* aContent) = 0;
  NS_IMETHOD SetBinding(nsIContent* aContent, nsXBLBinding* aBinding) = 0;

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
   * The binding manager assumes ownership of aList.
   */
  NS_IMETHOD SetContentListFor(nsIContent* aContent, nsVoidArray* aList)=0;

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
   * The binding manager assumes ownership of aList.
   */
  NS_IMETHOD SetAnonymousNodesFor(nsIContent* aContent, nsVoidArray* aList) = 0;

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
  virtual nsIContent* GetInsertionPoint(nsIContent* aParent,
                                        nsIContent* aChild,
                                        PRUint32* aIndex) = 0;

  /**
   * Return the unfiltered insertion point for the specified parent
   * element. If other filtered insertion points exist,
   * aMultipleInsertionPoints will be set to true.
   */
  virtual nsIContent* GetSingleInsertionPoint(nsIContent* aParent,
                                              PRUint32* aIndex,  
                                              PRBool* aMultipleInsertionPoints) = 0;

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, nsIURI* aURL) = 0;
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, nsIURI* aURL) = 0;
  NS_IMETHOD LoadBindingDocument(nsIDocument* aDocument, nsIURI* aURL,
                                 nsIDocument** aResult) = 0;

  NS_IMETHOD AddToAttachedQueue(nsXBLBinding* aBinding)=0;
  NS_IMETHOD ClearAttachedQueue()=0;
  NS_IMETHOD ProcessAttachedQueue()=0;

  NS_IMETHOD ExecuteDetachedHandlers()=0;

  NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)=0;
  NS_IMETHOD RemoveXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)=0;
  NS_IMETHOD GetXBLDocumentInfo(nsIURI* aURI, nsIXBLDocumentInfo** aResult)=0;

  NS_IMETHOD PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener) = 0;
  NS_IMETHOD GetLoadingDocListener(nsIURI* aURL, nsIStreamListener** aResult) = 0;
  NS_IMETHOD RemoveLoadingDocListener(nsIURI* aURL) = 0;

  NS_IMETHOD FlushSkinBindings() = 0;

  NS_IMETHOD GetBindingImplementation(nsIContent* aContent, REFNSIID aIID, void** aResult)=0;

  NS_IMETHOD ShouldBuildChildFrames(nsIContent* aContent, PRBool* aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIBindingManager, NS_IBINDING_MANAGER_IID)

#endif // nsIBinding_Manager_h__
