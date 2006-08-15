/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Alec Flett <alecf@netscape.com>
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

#ifndef nsBindingManager_h_
#define nsBindingManager_h_

#include "nsIBindingManager.h"
#include "nsIStyleRuleSupplier.h"
#include "nsStubDocumentObserver.h"
#include "pldhash.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"

class nsIContent;
class nsIXPConnectWrappedJS;
class nsIAtom;
class nsIDOMNodeList;
class nsVoidArray;
class nsIDocument;
class nsIURI;
class nsIXBLDocumentInfo;
class nsIStreamListener;
class nsStyleSet;

class nsBindingManager : public nsIBindingManager,
                         public nsIStyleRuleSupplier,
                         public nsIDocumentObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCUMENTOBSERVER

  nsBindingManager();
  ~nsBindingManager();

  virtual nsXBLBinding* GetBinding(nsIContent* aContent);
  NS_IMETHOD SetBinding(nsIContent* aContent, nsXBLBinding* aBinding);

  NS_IMETHOD GetInsertionParent(nsIContent* aContent, nsIContent** aResult);
  NS_IMETHOD SetInsertionParent(nsIContent* aContent, nsIContent* aResult);

  NS_IMETHOD GetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS** aResult);
  NS_IMETHOD SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aResult);

  NS_IMETHOD ChangeDocumentFor(nsIContent* aContent, nsIDocument* aOldDocument,
                               nsIDocument* aNewDocument);

  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult);

  NS_IMETHOD GetContentListFor(nsIContent* aContent, nsIDOMNodeList** aResult);
  NS_IMETHOD SetContentListFor(nsIContent* aContent, nsVoidArray* aList);
  NS_IMETHOD HasContentListFor(nsIContent* aContent, PRBool* aResult);

  NS_IMETHOD GetAnonymousNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);
  NS_IMETHOD SetAnonymousNodesFor(nsIContent* aContent, nsVoidArray* aList);

  NS_IMETHOD GetXBLChildNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);

  virtual nsIContent* GetInsertionPoint(nsIContent* aParent,
                                        nsIContent* aChild, PRUint32* aIndex);
  virtual nsIContent* GetSingleInsertionPoint(nsIContent* aParent,
                                              PRUint32* aIndex,  
                                              PRBool* aMultipleInsertionPoints);

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, nsIURI* aURL);
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, nsIURI* aURL);
  NS_IMETHOD LoadBindingDocument(nsIDocument* aBoundDoc, nsIURI* aURL,
                                 nsIDocument** aResult);

  NS_IMETHOD AddToAttachedQueue(nsXBLBinding* aBinding);
  NS_IMETHOD ClearAttachedQueue();
  NS_IMETHOD ProcessAttachedQueue();

  NS_IMETHOD ExecuteDetachedHandlers();

  NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo);
  NS_IMETHOD GetXBLDocumentInfo(nsIURI* aURI, nsIXBLDocumentInfo** aResult);
  NS_IMETHOD RemoveXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo);

  NS_IMETHOD PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener);
  NS_IMETHOD GetLoadingDocListener(nsIURI* aURL, nsIStreamListener** aResult);
  NS_IMETHOD RemoveLoadingDocListener(nsIURI* aURL);

  NS_IMETHOD FlushSkinBindings();

  NS_IMETHOD GetBindingImplementation(nsIContent* aContent, REFNSIID aIID, void** aResult);

  NS_IMETHOD ShouldBuildChildFrames(nsIContent* aContent, PRBool* aResult);

  virtual NS_HIDDEN_(void) AddObserver(nsIDocumentObserver* aObserver);

  virtual NS_HIDDEN_(PRBool) RemoveObserver(nsIDocumentObserver* aObserver);  

  // nsIStyleRuleSupplier
  NS_IMETHOD WalkRules(nsStyleSet* aStyleSet, 
                       nsIStyleRuleProcessor::EnumFunc aFunc,
                       RuleProcessorData* aData,
                       PRBool* aCutOffInheritance);

protected:
  nsresult GetXBLChildNodesInternal(nsIContent* aContent,
                                    nsIDOMNodeList** aResult,
                                    PRBool* aIsAnonymousContentList);
  nsresult GetAnonymousNodesInternal(nsIContent* aContent,
                                     nsIDOMNodeList** aResult,
                                     PRBool* aIsAnonymousContentList);

  nsIContent* GetEnclosingScope(nsIContent* aContent) {
    return aContent->GetBindingParent();
  }

  nsresult GetNestedInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult);

#define NS_BINDINGMANAGER_NOTIFY_OBSERVERS(func_, params_) \
  NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(mObservers, nsIDocumentObserver, \
                                     func_, params_);

// MEMBER VARIABLES
protected: 
  // A mapping from nsIContent* to the nsXBLBinding* that is
  // installed on that element.
  nsRefPtrHashtable<nsISupportsHashKey,nsXBLBinding> mBindingTable;

  // A mapping from nsIContent* to an nsIDOMNodeList*
  // (nsAnonymousContentList*).  This list contains an accurate
  // reflection of our *explicit* children (once intermingled with
  // insertion points) in the altered DOM.
  PLDHashTable mContentListTable;

  // A mapping from nsIContent* to an nsIDOMNodeList*
  // (nsAnonymousContentList*).  This list contains an accurate
  // reflection of our *anonymous* children (if and only if they are
  // intermingled with insertion points) in the altered DOM.  This
  // table is not used if no insertion points were defined directly
  // underneath a <content> tag in a binding.  The NodeList from the
  // <content> is used instead as a performance optimization.
  PLDHashTable mAnonymousNodesTable;

  // A mapping from nsIContent* to nsIContent*.  The insertion parent
  // is our one true parent in the transformed DOM.  This gives us a
  // more-or-less O(1) way of obtaining our transformed parent.
  PLDHashTable mInsertionParentTable;

  // A mapping from nsIContent* to nsIXPWrappedJS* (an XPConnect
  // wrapper for JS objects).  For XBL bindings that implement XPIDL
  // interfaces, and that get referred to from C++, this table caches
  // the XPConnect wrapper for the binding.  By caching it, I control
  // its lifetime, and I prevent a re-wrap of the same script object
  // (in the case where multiple bindings in an XBL inheritance chain
  // both implement an XPIDL interface).
  PLDHashTable mWrapperTable;

  // A mapping from a URL (a string) to nsIXBLDocumentInfo*.  This table
  // is the cache of all binding documents that have been loaded by a
  // given bound document.
  nsInterfaceHashtable<nsURIHashKey,nsIXBLDocumentInfo> mDocumentTable;

  // A mapping from a URL (a string) to a nsIStreamListener. This
  // table is the currently loading binding docs.  If they're in this
  // table, they have not yet finished loading.
  nsInterfaceHashtable<nsURIHashKey,nsIStreamListener> mLoadingDocTable;

  // Array of document observers who would like to be notified of content
  // appends/inserts after we update our data structures and of content removes
  // before we do so.
  nsTObserverArray<nsIDocumentObserver> mObservers;

  // A queue of binding attached event handlers that are awaiting execution.
  nsVoidArray mAttachedStack;
  PRBool mProcessingAttachedStack;
};

#endif
