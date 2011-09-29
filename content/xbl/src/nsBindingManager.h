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

#include "nsStubMutationObserver.h"
#include "pldhash.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"
#include "nsCycleCollectionParticipant.h"
#include "nsXBLBinding.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

class nsIContent;
class nsIXPConnectWrappedJS;
class nsIAtom;
class nsIDOMNodeList;
class nsIDocument;
class nsIURI;
class nsXBLDocumentInfo;
class nsIStreamListener;
class nsStyleSet;
class nsXBLBinding;
template<class E> class nsRefPtr;
typedef nsTArray<nsRefPtr<nsXBLBinding> > nsBindingList;
class nsIPrincipal;

class nsBindingManager : public nsStubMutationObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  nsBindingManager(nsIDocument* aDocument);
  ~nsBindingManager();

  nsXBLBinding* GetBinding(nsIContent* aContent);
  nsresult SetBinding(nsIContent* aContent, nsXBLBinding* aBinding);

  nsIContent* GetInsertionParent(nsIContent* aContent);
  nsresult SetInsertionParent(nsIContent* aContent, nsIContent* aResult);

  /**
   * Notify the binding manager that an element
   * has been removed from its document,
   * so that it can update any bindings or
   * nsIAnonymousContentCreator-created anonymous
   * content that may depend on the document.
   * @param aContent the element that's being moved
   * @param aOldDocument the old document in which the
   *   content resided.
   */
  void RemovedFromDocument(nsIContent* aContent, nsIDocument* aOldDocument)
  {
    if (aContent->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
      RemovedFromDocumentInternal(aContent, aOldDocument);
    }
  }
  void RemovedFromDocumentInternal(nsIContent* aContent,
                                   nsIDocument* aOldDocument);

  nsIAtom* ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID);

  /**
   * Return a list of all explicit children, including any children
   * that may have been inserted via XBL insertion points.
   */
  nsresult GetContentListFor(nsIContent* aContent, nsIDOMNodeList** aResult);

  /**
   * Non-COMy version of GetContentListFor.
   */
  nsINodeList* GetContentListFor(nsIContent* aContent);

  /**
   * Set the insertion point children for the specified element.
   * The binding manager assumes ownership of aList.
   */
  nsresult SetContentListFor(nsIContent* aContent,
                             nsInsertionPointList* aList);

  /**
   * Determine whether or not the explicit child list has been altered
   * by XBL insertion points.
   */
  bool HasContentListFor(nsIContent* aContent);

  /**
   * Return the nodelist of "anonymous" kids for this node.  This might
   * actually include some of the nodes actual DOM kids, if there are
   * <children> tags directly as kids of <content>.  This will only end up
   * returning a non-null list for nodes which have a binding attached.
   */
  nsresult GetAnonymousNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);

  /**
   * Same as above, but without the XPCOM goop
   */
  nsINodeList* GetAnonymousNodesFor(nsIContent* aContent);

  /**
   * Set the anonymous child content for the specified element.
   * The binding manager assumes ownership of aList.
   */
  nsresult SetAnonymousNodesFor(nsIContent* aContent,
                                nsInsertionPointList* aList);

  /**
   * Retrieves the anonymous list of children if the element has one;
   * otherwise, retrieves the list of explicit children. N.B. that if
   * the explicit child list has not been altered by XBL insertion
   * points, then aResult will be null.
   */
  nsresult GetXBLChildNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);

  /**
   * Non-COMy version of GetXBLChildNodesFor
   */
  nsINodeList* GetXBLChildNodesFor(nsIContent* aContent);

  /**
   * Given a parent element and a child content, determine where the
   * child content should be inserted in the parent element's
   * anonymous content tree. Specifically, aChild should be inserted
   * beneath aResult at the index specified by aIndex.
   */
  // XXXbz That's false.  The aIndex doesn't seem to accurately reflect
  // anything resembling reality in terms of inserting content.  It's really
  // only used to tell apart two different insertion points with the same
  // insertion parent when managing our internal data structures.  We really
  // shouldn't be handing it out in our public API, since it's not useful to
  // anyone.
  nsIContent* GetInsertionPoint(nsIContent* aParent,
                                const nsIContent* aChild, PRUint32* aIndex);

  /**
   * Return the unfiltered insertion point for the specified parent
   * element. If other filtered insertion points exist,
   * aMultipleInsertionPoints will be set to true.
   */
  nsIContent* GetSingleInsertionPoint(nsIContent* aParent, PRUint32* aIndex,
                                      bool* aMultipleInsertionPoints);

  nsIContent* GetNestedInsertionPoint(nsIContent* aParent,
                                      const nsIContent* aChild);
  nsIContent* GetNestedSingleInsertionPoint(nsIContent* aParent,
                                            bool* aMultipleInsertionPoints);

  nsresult AddLayeredBinding(nsIContent* aContent, nsIURI* aURL,
                             nsIPrincipal* aOriginPrincipal);
  nsresult RemoveLayeredBinding(nsIContent* aContent, nsIURI* aURL);
  nsresult LoadBindingDocument(nsIDocument* aBoundDoc, nsIURI* aURL,
                               nsIPrincipal* aOriginPrincipal);

  nsresult AddToAttachedQueue(nsXBLBinding* aBinding);
  void ProcessAttachedQueue(PRUint32 aSkipSize = 0);

  void ExecuteDetachedHandlers();

  nsresult PutXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo);
  nsXBLDocumentInfo* GetXBLDocumentInfo(nsIURI* aURI);
  void RemoveXBLDocumentInfo(nsXBLDocumentInfo* aDocumentInfo);

  nsresult PutLoadingDocListener(nsIURI* aURL, nsIStreamListener* aListener);
  nsIStreamListener* GetLoadingDocListener(nsIURI* aURL);
  void RemoveLoadingDocListener(nsIURI* aURL);

  void FlushSkinBindings();

  nsresult GetBindingImplementation(nsIContent* aContent, REFNSIID aIID, void** aResult);

  // Style rule methods
  nsresult WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                     RuleProcessorData* aData,
                     bool* aCutOffInheritance);

  void WalkAllRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                    RuleProcessorData* aData);
  /**
   * Do any processing that needs to happen as a result of a change in
   * the characteristics of the medium, and return whether this rule
   * processor's rules have changed (e.g., because of media queries).
   */
  nsresult MediumFeaturesChanged(nsPresContext* aPresContext,
                                 bool* aRulesChanged);

  void AppendAllSheets(nsTArray<nsCSSStyleSheet*>& aArray);

  NS_HIDDEN_(void) Traverse(nsIContent *aContent,
                            nsCycleCollectionTraversalCallback &cb);

  NS_DECL_CYCLE_COLLECTION_CLASS(nsBindingManager)

  // Notify the binding manager when an outermost update begins and
  // ends.  The end method can execute script.
  void BeginOutermostUpdate();
  void EndOutermostUpdate();

  // Called when the document is going away
  void DropDocumentReference();

protected:
  nsIXPConnectWrappedJS* GetWrappedJS(nsIContent* aContent);
  nsresult SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aResult);

  nsINodeList* GetXBLChildNodesInternal(nsIContent* aContent,
                                        bool* aIsAnonymousContentList);
  nsINodeList* GetAnonymousNodesInternal(nsIContent* aContent,
                                         bool* aIsAnonymousContentList);

  // Called by ContentAppended and ContentInserted to handle a single child
  // insertion.  aChild must not be null.  aContainer may be null.
  // aIndexInContainer is the index of the child in the parent.  aAppend is
  // true if this child is being appended, not inserted.
  void HandleChildInsertion(nsIContent* aContainer, nsIContent* aChild,
                            PRUint32 aIndexInContainer, bool aAppend);

  // For the given container under which a child is being added, given
  // insertion parent and given index of the child being inserted, find the
  // right nsXBLInsertionPoint and the right index in that insertion point to
  // insert it at.  If null is returned, aInsertionIndex might be garbage.
  // aAppend controls what should be returned as the aInsertionIndex if the
  // right index can't be found.  If true, the length of the insertion point
  // will be returned; otherwise 0 will be returned.
  nsXBLInsertionPoint* FindInsertionPointAndIndex(nsIContent* aContainer,
                                                  nsIContent* aInsertionParent,
                                                  PRUint32 aIndexInContainer,
                                                  PRInt32 aAppend,
                                                  PRInt32* aInsertionIndex);

  // Same as ProcessAttachedQueue, but also nulls out
  // mProcessAttachedQueueEvent
  void DoProcessAttachedQueue();

  // Post an event to process the attached queue.
  void PostProcessAttachedQueueEvent();

// MEMBER VARIABLES
protected: 
  void RemoveInsertionParent(nsIContent* aParent);
  // A mapping from nsIContent* to the nsXBLBinding* that is
  // installed on that element.
  nsRefPtrHashtable<nsISupportsHashKey,nsXBLBinding> mBindingTable;

  // A mapping from nsIContent* to an nsAnonymousContentList*.  This
  // list contains an accurate reflection of our *explicit* children
  // (once intermingled with insertion points) in the altered DOM.
  // There is an entry for a content node in this table only if that
  // content node has some <children> kids.
  PLDHashTable mContentListTable;

  // A mapping from nsIContent* to an nsAnonymousContentList*.  This
  // list contains an accurate reflection of our *anonymous* children
  // (if and only if they are intermingled with insertion points) in
  // the altered DOM.  This table is not used if no insertion points
  // were defined directly underneath a <content> tag in a binding.
  // The NodeList from the <content> is used instead as a performance
  // optimization.  There is an entry for a content node in this table
  // only if that content node has a binding with a <content> attached
  // and this <content> contains <children> elements directly.
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

  // A mapping from a URL (a string) to nsXBLDocumentInfo*.  This table
  // is the cache of all binding documents that have been loaded by a
  // given bound document.
  nsRefPtrHashtable<nsURIHashKey,nsXBLDocumentInfo> mDocumentTable;

  // A mapping from a URL (a string) to a nsIStreamListener. This
  // table is the currently loading binding docs.  If they're in this
  // table, they have not yet finished loading.
  nsInterfaceHashtable<nsURIHashKey,nsIStreamListener> mLoadingDocTable;

  // A queue of binding attached event handlers that are awaiting execution.
  nsBindingList mAttachedStack;
  bool mProcessingAttachedStack;
  bool mDestroyed;
  PRUint32 mAttachedStackSizeOnOutermost;

  // Our posted event to process the attached queue, if any
  friend class nsRunnableMethod<nsBindingManager>;
  nsRefPtr< nsRunnableMethod<nsBindingManager> > mProcessAttachedQueueEvent;

  // Our document.  This is a weak ref; the document owns us
  nsIDocument* mDocument; 
};

#endif
