/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBindingManager_h_
#define nsBindingManager_h_

#include "nsAutoPtr.h"
#include "nsIContent.h"
#include "nsStubMutationObserver.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"
#include "nsCycleCollectionParticipant.h"
#include "nsXBLBinding.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/StyleSheetHandle.h"

struct ElementDependentRuleProcessorData;
class nsIXPConnectWrappedJS;
class nsIAtom;
class nsIDOMNodeList;
class nsIDocument;
class nsIURI;
class nsXBLDocumentInfo;
class nsIStreamListener;
class nsXBLBinding;
typedef nsTArray<RefPtr<nsXBLBinding> > nsBindingList;
class nsIPrincipal;
class nsITimer;

class nsBindingManager final : public nsStubMutationObserver
{
  ~nsBindingManager();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  explicit nsBindingManager(nsIDocument* aDocument);

  nsXBLBinding* GetBindingWithContent(nsIContent* aContent);

  void AddBoundContent(nsIContent* aContent);
  void RemoveBoundContent(nsIContent* aContent);

  /**
   * Notify the binding manager that an element
   * has been removed from its document,
   * so that it can update any bindings or
   * nsIAnonymousContentCreator-created anonymous
   * content that may depend on the document.
   * @param aContent the element that's being moved
   * @param aOldDocument the old document in which the
   *   content resided.
   * @param aDestructorHandling whether or not to run the possible XBL
   *        destructor.
   */

 enum DestructorHandling {
   eRunDtor,
   eDoNotRunDtor
 };
  void RemovedFromDocument(nsIContent* aContent, nsIDocument* aOldDocument,
                           DestructorHandling aDestructorHandling)
  {
    if (aContent->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
      RemovedFromDocumentInternal(aContent, aOldDocument, aDestructorHandling);
    }
  }
  void RemovedFromDocumentInternal(nsIContent* aContent,
                                   nsIDocument* aOldDocument,
                                   DestructorHandling aDestructorHandling);

  nsIAtom* ResolveTag(nsIContent* aContent, int32_t* aNameSpaceID);

  /**
   * Return the nodelist of "anonymous" kids for this node.  This might
   * actually include some of the nodes actual DOM kids, if there are
   * <children> tags directly as kids of <content>.  This will only end up
   * returning a non-null list for nodes which have a binding attached.
   */
  nsresult GetAnonymousNodesFor(nsIContent* aContent, nsIDOMNodeList** aResult);
  nsINodeList* GetAnonymousNodesFor(nsIContent* aContent);

  nsresult ClearBinding(nsIContent* aContent);
  nsresult LoadBindingDocument(nsIDocument* aBoundDoc, nsIURI* aURL,
                               nsIPrincipal* aOriginPrincipal);

  nsresult AddToAttachedQueue(nsXBLBinding* aBinding);
  void RemoveFromAttachedQueue(nsXBLBinding* aBinding);
  void ProcessAttachedQueue(uint32_t aSkipSize = 0)
  {
    if (mProcessingAttachedStack || mAttachedStack.Length() <= aSkipSize) {
      return;
    }

    ProcessAttachedQueueInternal(aSkipSize);
  }
private:
  void ProcessAttachedQueueInternal(uint32_t aSkipSize);

public:

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
                     ElementDependentRuleProcessorData* aData,
                     bool* aCutOffInheritance);

  void WalkAllRules(nsIStyleRuleProcessor::EnumFunc aFunc,
                    ElementDependentRuleProcessorData* aData);
  /**
   * Do any processing that needs to happen as a result of a change in
   * the characteristics of the medium, and return whether this rule
   * processor's rules have changed (e.g., because of media queries).
   */
  nsresult MediumFeaturesChanged(nsPresContext* aPresContext,
                                 bool* aRulesChanged);

  void AppendAllSheets(nsTArray<mozilla::StyleSheetHandle>& aArray);

  void Traverse(nsIContent *aContent,
                            nsCycleCollectionTraversalCallback &cb);

  NS_DECL_CYCLE_COLLECTION_CLASS(nsBindingManager)

  // Notify the binding manager when an outermost update begins and
  // ends.  The end method can execute script.
  void BeginOutermostUpdate()
  {
    mAttachedStackSizeOnOutermost = mAttachedStack.Length();
  }

  void EndOutermostUpdate()
  {
    if (!mProcessingAttachedStack) {
      ProcessAttachedQueue(mAttachedStackSizeOnOutermost);
      mAttachedStackSizeOnOutermost = 0;
    }
  }

  // When removing an insertion point or a parent of one, clear the insertion
  // points and their insertion parents.
  void ClearInsertionPointsRecursively(nsIContent* aContent);

  // Called when the document is going away
  void DropDocumentReference();

  nsIContent* FindNestedInsertionPoint(nsIContent* aContainer,
                                       nsIContent* aChild);

  nsIContent* FindNestedSingleInsertionPoint(nsIContent* aContainer, bool* aMulti);

protected:
  nsIXPConnectWrappedJS* GetWrappedJS(nsIContent* aContent);
  nsresult SetWrappedJS(nsIContent* aContent, nsIXPConnectWrappedJS* aResult);

  // Called by ContentAppended and ContentInserted to handle a single child
  // insertion.  aChild must not be null.  aContainer may be null.
  // aIndexInContainer is the index of the child in the parent.  aAppend is
  // true if this child is being appended, not inserted.
  void HandleChildInsertion(nsIContent* aContainer, nsIContent* aChild,
                            uint32_t aIndexInContainer, bool aAppend);

  // Same as ProcessAttachedQueue, but also nulls out
  // mProcessAttachedQueueEvent
  void DoProcessAttachedQueue();

  // Post an event to process the attached queue.
  void PostProcessAttachedQueueEvent();

  // Call PostProcessAttachedQueueEvent() on a timer.
  static void PostPAQEventCallback(nsITimer* aTimer, void* aClosure);

// MEMBER VARIABLES
protected:
  // A set of nsIContent that currently have a binding installed.
  nsAutoPtr<nsTHashtable<nsRefPtrHashKey<nsIContent> > > mBoundContentSet;

  // A mapping from nsIContent* to nsIXPWrappedJS* (an XPConnect
  // wrapper for JS objects).  For XBL bindings that implement XPIDL
  // interfaces, and that get referred to from C++, this table caches
  // the XPConnect wrapper for the binding.  By caching it, I control
  // its lifetime, and I prevent a re-wrap of the same script object
  // (in the case where multiple bindings in an XBL inheritance chain
  // both implement an XPIDL interface).
  typedef nsInterfaceHashtable<nsISupportsHashKey, nsIXPConnectWrappedJS> WrapperHashtable;
  nsAutoPtr<WrapperHashtable> mWrapperTable;

  // A mapping from a URL (a string) to nsXBLDocumentInfo*.  This table
  // is the cache of all binding documents that have been loaded by a
  // given bound document.
  nsAutoPtr<nsRefPtrHashtable<nsURIHashKey,nsXBLDocumentInfo> > mDocumentTable;

  // A mapping from a URL (a string) to a nsIStreamListener. This
  // table is the currently loading binding docs.  If they're in this
  // table, they have not yet finished loading.
  nsAutoPtr<nsInterfaceHashtable<nsURIHashKey,nsIStreamListener> > mLoadingDocTable;

  // A queue of binding attached event handlers that are awaiting execution.
  nsBindingList mAttachedStack;
  bool mProcessingAttachedStack;
  bool mDestroyed;
  uint32_t mAttachedStackSizeOnOutermost;

  // Our posted event to process the attached queue, if any
  friend class nsRunnableMethod<nsBindingManager>;
  RefPtr< nsRunnableMethod<nsBindingManager> > mProcessAttachedQueueEvent;

  // Our document.  This is a weak ref; the document owns us
  nsIDocument* mDocument;
};

#endif
