/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMutationObserver_h
#define nsDOMMutationObserver_h

#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptContext.h"
#include "nsStubMutationObserver.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsIVariant.h"
#include "nsContentList.h"
#include "mozilla/dom/Element.h"
#include "nsClassHashtable.h"
#include "nsNodeUtils.h"
#include "nsIDOMMutationEvent.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/MutationObserverBinding.h"
#include "nsIDocument.h"

class nsDOMMutationObserver;
using mozilla::dom::MutationObservingInfoInitializer;

class nsDOMMutationRecord : public nsISupports,
                            public nsWrapperCache
{
public:
  nsDOMMutationRecord(const nsAString& aType, nsISupports* aOwner)
  : mType(aType), mOwner(aOwner)
  {
    mAttrName.SetIsVoid(PR_TRUE);
    mAttrNamespace.SetIsVoid(PR_TRUE);
    mPrevValue.SetIsVoid(PR_TRUE);
    SetIsDOMBinding();
  }
  virtual ~nsDOMMutationRecord() {}

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::MutationRecordBinding::Wrap(aCx, aScope, this);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMMutationRecord)

  void GetType(nsString& aRetVal) const
  {
    aRetVal = mType;
  }

  nsINode* GetTarget() const
  {
    return mTarget;
  }

  nsINodeList* AddedNodes();

  nsINodeList* RemovedNodes();

  nsINode* GetPreviousSibling() const
  {
    return mPreviousSibling;
  }

  nsINode* GetNextSibling() const
  {
    return mNextSibling;
  }

  void GetAttributeName(nsString& aRetVal) const
  {
    aRetVal = mAttrName;
  }

  void GetAttributeNamespace(nsString& aRetVal) const
  {
    aRetVal = mAttrNamespace;
  }

  void GetOldValue(nsString& aRetVal) const
  {
    aRetVal = mPrevValue;
  }

  nsCOMPtr<nsINode>             mTarget;
  nsString                      mType;
  nsString                      mAttrName;
  nsString                      mAttrNamespace;
  nsString                      mPrevValue;
  nsRefPtr<nsSimpleContentList> mAddedNodes;
  nsRefPtr<nsSimpleContentList> mRemovedNodes;
  nsCOMPtr<nsINode>             mPreviousSibling;
  nsCOMPtr<nsINode>             mNextSibling;
  nsCOMPtr<nsISupports>         mOwner;
};
 
// Base class just prevents direct access to
// members to make sure we go through getters/setters.
class nsMutationReceiverBase : public nsStubMutationObserver
{
public:
  virtual ~nsMutationReceiverBase() { }

  nsDOMMutationObserver* Observer();
  nsINode* Target() { return mParent ? mParent->Target() : mTarget; }
  nsINode* RegisterTarget() { return mRegisterTarget; }

  bool Subtree() { return mParent ? mParent->Subtree() : mSubtree; }
  void SetSubtree(bool aSubtree)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mSubtree = aSubtree;
  }

  bool ChildList() { return mParent ? mParent->ChildList() : mChildList; }
  void SetChildList(bool aChildList)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mChildList = aChildList;
  }

  bool CharacterData()
  {
    return mParent ? mParent->CharacterData() : mCharacterData;
  }
  void SetCharacterData(bool aCharacterData)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mCharacterData = aCharacterData;
  }

  bool CharacterDataOldValue()
  {
    return mParent ? mParent->CharacterDataOldValue() : mCharacterDataOldValue;
  }
  void SetCharacterDataOldValue(bool aOldValue)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mCharacterDataOldValue = aOldValue;
  }

  bool Attributes() { return mParent ? mParent->Attributes() : mAttributes; }
  void SetAttributes(bool aAttributes)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mAttributes = aAttributes;
  }

  bool AllAttributes()
  {
    return mParent ? mParent->AllAttributes()
                   : mAllAttributes;
  }
  void SetAllAttributes(bool aAll)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mAllAttributes = aAll;
  }

  bool AttributeOldValue() {
    return mParent ? mParent->AttributeOldValue()
                   : mAttributeOldValue;
  }
  void SetAttributeOldValue(bool aOldValue)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mAttributeOldValue = aOldValue;
  }

  nsCOMArray<nsIAtom>& AttributeFilter() { return mAttributeFilter; }
  void SetAttributeFilter(nsCOMArray<nsIAtom>& aFilter)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mAttributeFilter.Clear();
    mAttributeFilter.AppendObjects(aFilter);
  }

  void AddClone(nsMutationReceiverBase* aClone)
  {
    mTransientReceivers.AppendObject(aClone);
  }

  void RemoveClone(nsMutationReceiverBase* aClone)
  {
    mTransientReceivers.RemoveObject(aClone);
  }
  
protected:
  nsMutationReceiverBase(nsINode* aTarget, nsDOMMutationObserver* aObserver)
  : mTarget(aTarget), mObserver(aObserver), mRegisterTarget(aTarget)
  {
    mRegisterTarget->AddMutationObserver(this);
    mRegisterTarget->SetMayHaveDOMMutationObserver();
    mRegisterTarget->OwnerDoc()->SetMayHaveDOMMutationObservers();
  }

  nsMutationReceiverBase(nsINode* aRegisterTarget,
                         nsMutationReceiverBase* aParent)
  : mTarget(nullptr), mObserver(nullptr), mParent(aParent),
    mRegisterTarget(aRegisterTarget), mKungFuDeathGrip(aParent->Target())
  {
    NS_ASSERTION(mParent->Subtree(), "Should clone a non-subtree observer!");
    mRegisterTarget->AddMutationObserver(this);
    mRegisterTarget->SetMayHaveDOMMutationObserver();
    mRegisterTarget->OwnerDoc()->SetMayHaveDOMMutationObservers();
  }

  bool ObservesAttr(mozilla::dom::Element* aElement,
                    int32_t aNameSpaceID,
                    nsIAtom* aAttr)
  {
    if (mParent) {
      return mParent->ObservesAttr(aElement, aNameSpaceID, aAttr);
    }
    if (!Attributes() || (!Subtree() && aElement != Target())) {
      return false;
    }
    if (AllAttributes()) {
      return true;
    }

    if (aNameSpaceID != kNameSpaceID_None) {
      return false;
    }

    nsCOMArray<nsIAtom>& filters = AttributeFilter();
    for (int32_t i = 0; i < filters.Count(); ++i) {
      if (filters[i] == aAttr) {
        return true;
      }
    }
    return false;
  }

  // The target for the MutationObserver.observe() method.
  nsINode*                           mTarget;
  nsDOMMutationObserver*             mObserver;
  nsRefPtr<nsMutationReceiverBase>   mParent; // Cleared after microtask.
  // The node to which Gecko-internal nsIMutationObserver was registered to.
  // This is different than mTarget when dealing with transient observers.
  nsINode*                           mRegisterTarget;
  nsCOMArray<nsMutationReceiverBase> mTransientReceivers;
  // While we have transient receivers, keep the original mutation receiver
  // alive so it doesn't go away and disconnect all its transient receivers.
  nsCOMPtr<nsINode>                  mKungFuDeathGrip;
  
private:
  bool                               mSubtree;
  bool                               mChildList;
  bool                               mCharacterData;
  bool                               mCharacterDataOldValue;
  bool                               mAttributes;
  bool                               mAllAttributes;
  bool                               mAttributeOldValue;
  nsCOMArray<nsIAtom>                mAttributeFilter;
};


class nsMutationReceiver : public nsMutationReceiverBase
{
public:
  nsMutationReceiver(nsINode* aTarget, nsDOMMutationObserver* aObserver);

  nsMutationReceiver(nsINode* aRegisterTarget, nsMutationReceiverBase* aParent)
  : nsMutationReceiverBase(aRegisterTarget, aParent)
  {
    NS_ASSERTION(!static_cast<nsMutationReceiver*>(aParent)->GetParent(),
                 "Shouldn't create deep observer hierarchies!");
    aParent->AddClone(this);
  }

  virtual ~nsMutationReceiver() { Disconnect(false); }

  nsMutationReceiver* GetParent()
  {
    return static_cast<nsMutationReceiver*>(mParent.get());
  }

  void RemoveClones()
  {
    for (int32_t i = 0; i < mTransientReceivers.Count(); ++i) {
      nsMutationReceiver* r =
        static_cast<nsMutationReceiver*>(mTransientReceivers[i]);
      r->DisconnectTransientReceiver();
    }
    mTransientReceivers.Clear();
  }

  void DisconnectTransientReceiver()
  {
    if (mRegisterTarget) {
      mRegisterTarget->RemoveMutationObserver(this);
      mRegisterTarget = nullptr;
    }

    mParent = nullptr;
    NS_ASSERTION(!mTarget, "Should not have mTarget");
    NS_ASSERTION(!mObserver, "Should not have mObserver");
  }

  void Disconnect(bool aRemoveFromObserver);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  NS_DECL_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  virtual void AttributeSetToCurrentValue(nsIDocument* aDocument,
                                          mozilla::dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsIAtom* aAttribute) MOZ_OVERRIDE
  {
    // We can reuse AttributeWillChange implementation.
    AttributeWillChange(aDocument, aElement, aNameSpaceID, aAttribute,
                        nsIDOMMutationEvent::MODIFICATION);
  }
};

#define NS_DOM_MUTATION_OBSERVER_IID \
{ 0x0c3b91f8, 0xcc3b, 0x4b08, \
  { 0x9e, 0xab, 0x07, 0x47, 0xa9, 0xe4, 0x65, 0xb4 } }

class nsDOMMutationObserver : public nsISupports,
                              public nsWrapperCache
{
public:
  nsDOMMutationObserver(already_AddRefed<nsPIDOMWindow> aOwner,
                        mozilla::dom::MutationCallback& aCb)
  : mOwner(aOwner), mCallback(&aCb), mWaitingForRun(false), mId(++sCount)
  {
    SetIsDOMBinding();
  }
  virtual ~nsDOMMutationObserver();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMMutationObserver)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_MUTATION_OBSERVER_IID)

  static already_AddRefed<nsDOMMutationObserver>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              mozilla::dom::MutationCallback& aCb,
              mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::MutationObserverBinding::Wrap(aCx, aScope, this);
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  void Observe(nsINode& aTarget,
               const mozilla::dom::MutationObserverInit& aOptions,
               mozilla::ErrorResult& aRv);

  void Disconnect();

  void TakeRecords(nsTArray<nsRefPtr<nsDOMMutationRecord> >& aRetVal);

  void HandleMutation();

  void GetObservingInfo(nsTArray<Nullable<MutationObservingInfoInitializer> >& aResult);

  mozilla::dom::MutationCallback* MutationCallback() { return mCallback; }

  // static methods
  static void HandleMutations()
  {
    if (sScheduledMutationObservers) {
      HandleMutationsInternal();
    }
  }

  static void EnterMutationHandling();
  static void LeaveMutationHandling();

  static void Shutdown();
protected:
  friend class nsMutationReceiver;
  friend class nsAutoMutationBatch;
  nsMutationReceiver* GetReceiverFor(nsINode* aNode, bool aMayCreate);
  void RemoveReceiver(nsMutationReceiver* aReceiver);

  already_AddRefed<nsIVariant> TakeRecords();

  void GetAllSubtreeObserversFor(nsINode* aNode,
                                 nsTArray<nsMutationReceiver*>& aObservers);
  void ScheduleForRun();
  void RescheduleForRun();

  nsDOMMutationRecord* CurrentRecord(const nsAString& aType);
  bool HasCurrentRecord(const nsAString& aType);

  bool Suppressed()
  {
    if (mOwner) {
      nsCOMPtr<nsIDocument> d = mOwner->GetExtantDoc();
      return d && d->IsInSyncOperation();
    }
    return false;
  }

  static void HandleMutationsInternal();

  static void AddCurrentlyHandlingObserver(nsDOMMutationObserver* aObserver);

  nsCOMPtr<nsPIDOMWindow>                            mOwner;

  nsCOMArray<nsMutationReceiver>                     mReceivers;
  nsClassHashtable<nsISupportsHashKey,
                   nsCOMArray<nsMutationReceiver> >  mTransientReceivers;
  // MutationRecords which are being constructed.
  nsAutoTArray<nsDOMMutationRecord*, 4>              mCurrentMutations;
  // MutationRecords which will be handed to the callback at the end of
  // the microtask.
  nsTArray<nsRefPtr<nsDOMMutationRecord> >           mPendingMutations;
  nsRefPtr<mozilla::dom::MutationCallback>           mCallback;

  bool                                               mWaitingForRun;

  uint64_t                                           mId;

  static uint64_t                                    sCount;
  static nsTArray<nsRefPtr<nsDOMMutationObserver> >* sScheduledMutationObservers;
  static nsDOMMutationObserver*                      sCurrentObserver;

  static uint32_t                                    sMutationLevel;
  static nsAutoTArray<nsTArray<nsRefPtr<nsDOMMutationObserver> >, 4>*
                                                     sCurrentlyHandlingObservers;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDOMMutationObserver, NS_DOM_MUTATION_OBSERVER_IID)

class nsAutoMutationBatch
{
public:
  nsAutoMutationBatch()
  : mPreviousBatch(nullptr), mBatchTarget(nullptr), mRemovalDone(false),
    mFromFirstToLast(false), mAllowNestedBatches(false)    
  {
  }

  nsAutoMutationBatch(nsINode* aTarget, bool aFromFirstToLast,
                      bool aAllowNestedBatches)
  : mPreviousBatch(nullptr), mBatchTarget(nullptr), mRemovalDone(false),
    mFromFirstToLast(false), mAllowNestedBatches(false)
  {
    Init(aTarget, aFromFirstToLast, aAllowNestedBatches);
  }

  void Init(nsINode* aTarget, bool aFromFirstToLast, bool aAllowNestedBatches)
  {
    if (aTarget && aTarget->OwnerDoc()->MayHaveDOMMutationObservers()) {
      if (sCurrentBatch && !sCurrentBatch->mAllowNestedBatches) {
        return;
      }
      mBatchTarget = aTarget;
      mFromFirstToLast = aFromFirstToLast;
      mAllowNestedBatches = aAllowNestedBatches;
      mPreviousBatch = sCurrentBatch;
      sCurrentBatch = this;
      nsDOMMutationObserver::EnterMutationHandling();
    }
  }

  void RemovalDone() { mRemovalDone = true; }
  static bool IsRemovalDone() { return sCurrentBatch->mRemovalDone; }

  void SetPrevSibling(nsINode* aNode) { mPrevSibling = aNode; }
  void SetNextSibling(nsINode* aNode) { mNextSibling = aNode; }

  void Done();

  ~nsAutoMutationBatch() { NodesAdded(); }

  static bool IsBatching()
  {
    return !!sCurrentBatch;
  }

  static nsAutoMutationBatch* GetCurrentBatch()
  {
    return sCurrentBatch;
  }

  static void UpdateObserver(nsDOMMutationObserver* aObserver,
                             bool aWantsChildList)
  {
    uint32_t l = sCurrentBatch->mObservers.Length();
    for (uint32_t i = 0; i < l; ++i) {
      if (sCurrentBatch->mObservers[i].mObserver == aObserver) {
        if (aWantsChildList) {
          sCurrentBatch->mObservers[i].mWantsChildList = aWantsChildList;
        } 
        return;
      }
    }
    BatchObserver* bo = sCurrentBatch->mObservers.AppendElement(); 
    bo->mObserver = aObserver;
    bo->mWantsChildList = aWantsChildList;
  }


  static nsINode* GetBatchTarget() { return sCurrentBatch->mBatchTarget; }

  // Mutation receivers notify the batch about removed child nodes.
  static void NodeRemoved(nsIContent* aChild)
  {
    if (IsBatching() && !sCurrentBatch->mRemovalDone) {
      uint32_t len = sCurrentBatch->mRemovedNodes.Length();
      if (!len ||
          sCurrentBatch->mRemovedNodes[len - 1] != aChild) {
        sCurrentBatch->mRemovedNodes.AppendElement(aChild);
      }
    }
  }

  // Called after new child nodes have been added to the batch target.
  void NodesAdded()
  {
    if (sCurrentBatch != this) {
      return;
    }

    nsIContent* c =
      mPrevSibling ? mPrevSibling->GetNextSibling() :
                     mBatchTarget->GetFirstChild();
    for (; c != mNextSibling; c = c->GetNextSibling()) {
      mAddedNodes.AppendElement(c);
    }
    Done();
  }

private:
  struct BatchObserver
  {
    nsDOMMutationObserver* mObserver;
    bool                   mWantsChildList;
  };
  
  static nsAutoMutationBatch* sCurrentBatch;
  nsAutoMutationBatch* mPreviousBatch;
  nsAutoTArray<BatchObserver, 2> mObservers;
  nsTArray<nsCOMPtr<nsIContent> > mRemovedNodes;
  nsTArray<nsCOMPtr<nsIContent> > mAddedNodes;
  nsINode* mBatchTarget;
  bool mRemovalDone;
  bool mFromFirstToLast;
  bool mAllowNestedBatches;
  nsCOMPtr<nsINode> mPrevSibling;
  nsCOMPtr<nsINode> mNextSibling;
};

inline
nsDOMMutationObserver*
nsMutationReceiverBase::Observer()
{
  return mParent ?
    mParent->Observer() : static_cast<nsDOMMutationObserver*>(mObserver);
}

#endif
