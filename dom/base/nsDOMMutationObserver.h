/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMutationObserver_h
#define nsDOMMutationObserver_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptContext.h"
#include "nsStubAnimationObserver.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIVariant.h"
#include "nsContentList.h"
#include "mozilla/dom/Element.h"
#include "nsClassHashtable.h"
#include "nsNodeUtils.h"
#include "nsIDOMMutationEvent.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/MutationObserverBinding.h"
#include "nsIDocument.h"
#include "mozilla/dom/Animation.h"
#include "nsIAnimationObserver.h"
#include "nsGlobalWindow.h"

class nsDOMMutationObserver;
using mozilla::dom::MutationObservingInfo;

class nsDOMMutationRecord final : public nsISupports,
                                  public nsWrapperCache
{
  virtual ~nsDOMMutationRecord() {}

public:
  typedef nsTArray<RefPtr<mozilla::dom::Animation>> AnimationArray;

  nsDOMMutationRecord(nsAtom* aType, nsISupports* aOwner)
  : mType(aType), mAttrNamespace(VoidString()), mPrevValue(VoidString()), mOwner(aOwner)
  {
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::MutationRecordBinding::Wrap(aCx, this, aGivenProto);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMMutationRecord)

  void GetType(mozilla::dom::DOMString& aRetVal) const
  {
    aRetVal.SetKnownLiveAtom(mType, mozilla::dom::DOMString::eNullNotExpected);
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

  void GetAttributeName(mozilla::dom::DOMString& aRetVal) const
  {
    aRetVal.SetKnownLiveAtom(mAttrName,
                             mozilla::dom::DOMString::eTreatNullAsNull);
  }

  void GetAttributeNamespace(mozilla::dom::DOMString& aRetVal) const
  {
    aRetVal.SetKnownLiveString(mAttrNamespace);
  }

  void GetOldValue(mozilla::dom::DOMString& aRetVal) const
  {
    aRetVal.SetKnownLiveString(mPrevValue);
  }

  void GetAddedAnimations(AnimationArray& aRetVal) const
  {
    aRetVal = mAddedAnimations;
  }

  void GetRemovedAnimations(AnimationArray& aRetVal) const
  {
    aRetVal = mRemovedAnimations;
  }

  void GetChangedAnimations(AnimationArray& aRetVal) const
  {
    aRetVal = mChangedAnimations;
  }

  nsCOMPtr<nsINode>             mTarget;
  RefPtr<nsAtom>             mType;
  RefPtr<nsAtom>             mAttrName;
  nsString                      mAttrNamespace;
  nsString                      mPrevValue;
  RefPtr<nsSimpleContentList> mAddedNodes;
  RefPtr<nsSimpleContentList> mRemovedNodes;
  nsCOMPtr<nsINode>             mPreviousSibling;
  nsCOMPtr<nsINode>             mNextSibling;
  AnimationArray                mAddedAnimations;
  AnimationArray                mRemovedAnimations;
  AnimationArray                mChangedAnimations;

  RefPtr<nsDOMMutationRecord> mNext;
  nsCOMPtr<nsISupports>         mOwner;
};

// Base class just prevents direct access to
// members to make sure we go through getters/setters.
class nsMutationReceiverBase : public nsStubAnimationObserver
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

  bool NativeAnonymousChildList()
  {
    return mParent ? mParent->NativeAnonymousChildList() : mNativeAnonymousChildList;
  }
  void SetNativeAnonymousChildList(bool aOldValue)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mNativeAnonymousChildList = aOldValue;
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

  bool Animations() { return mParent ? mParent->Animations() : mAnimations; }
  void SetAnimations(bool aAnimations)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mAnimations = aAnimations;
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

  nsTArray<RefPtr<nsAtom>>& AttributeFilter() { return mAttributeFilter; }
  void SetAttributeFilter(nsTArray<RefPtr<nsAtom>>&& aFilter)
  {
    NS_ASSERTION(!mParent, "Shouldn't have parent");
    mAttributeFilter.Clear();
    mAttributeFilter = mozilla::Move(aFilter);
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
    : mTarget(aTarget)
    , mObserver(aObserver)
    , mRegisterTarget(aTarget)
    , mSubtree(false)
    , mChildList(false)
    , mCharacterData(false)
    , mCharacterDataOldValue(false)
    , mNativeAnonymousChildList(false)
    , mAttributes(false)
    , mAllAttributes(false)
    , mAttributeOldValue(false)
    , mAnimations(false)
  {
  }

  nsMutationReceiverBase(nsINode* aRegisterTarget,
                         nsMutationReceiverBase* aParent)
    : mTarget(nullptr)
    , mObserver(nullptr)
    , mParent(aParent)
    , mRegisterTarget(aRegisterTarget)
    , mKungFuDeathGrip(aParent->Target())
    , mSubtree(false)
    , mChildList(false)
    , mCharacterData(false)
    , mCharacterDataOldValue(false)
    , mNativeAnonymousChildList(false)
    , mAttributes(false)
    , mAllAttributes(false)
    , mAttributeOldValue(false)
    , mAnimations(false)
  {
    NS_ASSERTION(mParent->Subtree(), "Should clone a non-subtree observer!");
  }

  virtual void AddMutationObserver() = 0;

  void AddObserver()
  {
    AddMutationObserver();
    mRegisterTarget->SetMayHaveDOMMutationObserver();
    mRegisterTarget->OwnerDoc()->SetMayHaveDOMMutationObservers();
  }

  bool IsObservable(nsIContent* aContent);

  bool ObservesAttr(nsINode* aRegisterTarget,
                    mozilla::dom::Element* aElement,
                    int32_t aNameSpaceID,
                    nsAtom* aAttr)
  {
    if (mParent) {
      return mParent->ObservesAttr(aRegisterTarget, aElement, aNameSpaceID, aAttr);
    }
    if (!Attributes() ||
        (!Subtree() && aElement != Target()) ||
        (Subtree() && aRegisterTarget->SubtreeRoot() != aElement->SubtreeRoot()) ||
        !IsObservable(aElement)) {
      return false;
    }
    if (AllAttributes()) {
      return true;
    }

    if (aNameSpaceID != kNameSpaceID_None) {
      return false;
    }

    nsTArray<RefPtr<nsAtom>>& filters = AttributeFilter();
    for (size_t i = 0; i < filters.Length(); ++i) {
      if (filters[i] == aAttr) {
        return true;
      }
    }
    return false;
  }

  // The target for the MutationObserver.observe() method.
  nsINode*                           mTarget;
  nsDOMMutationObserver*             mObserver;
  RefPtr<nsMutationReceiverBase>   mParent; // Cleared after microtask.
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
  bool                               mNativeAnonymousChildList;
  bool                               mAttributes;
  bool                               mAllAttributes;
  bool                               mAttributeOldValue;
  bool                               mAnimations;
  nsTArray<RefPtr<nsAtom>>          mAttributeFilter;
};


class nsMutationReceiver : public nsMutationReceiverBase
{
protected:
  virtual ~nsMutationReceiver() { Disconnect(false); }

public:
  static nsMutationReceiver* Create(nsINode* aTarget,
                                    nsDOMMutationObserver* aObserver)
  {
    nsMutationReceiver* r = new nsMutationReceiver(aTarget, aObserver);
    r->AddObserver();
    return r;
  }

  static nsMutationReceiver* Create(nsINode* aRegisterTarget,
                                    nsMutationReceiverBase* aParent)
  {
    nsMutationReceiver* r = new nsMutationReceiver(aRegisterTarget, aParent);
    aParent->AddClone(r);
    r->AddObserver();
    return r;
  }

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

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_NATIVEANONYMOUSCHILDLISTCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  virtual void AttributeSetToCurrentValue(nsIDocument* aDocument,
                                          mozilla::dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsAtom* aAttribute) override
  {
    // We can reuse AttributeWillChange implementation.
    AttributeWillChange(aDocument, aElement, aNameSpaceID, aAttribute,
                        nsIDOMMutationEvent::MODIFICATION, nullptr);
  }

protected:
  nsMutationReceiver(nsINode* aTarget, nsDOMMutationObserver* aObserver);

  nsMutationReceiver(nsINode* aRegisterTarget, nsMutationReceiverBase* aParent)
  : nsMutationReceiverBase(aRegisterTarget, aParent)
  {
    NS_ASSERTION(!static_cast<nsMutationReceiver*>(aParent)->GetParent(),
                 "Shouldn't create deep observer hierarchies!");
  }

  virtual void AddMutationObserver() override
  {
    mRegisterTarget->AddMutationObserver(this);
  }
};

class nsAnimationReceiver : public nsMutationReceiver
{
public:
  static nsAnimationReceiver* Create(nsINode* aTarget,
                                     nsDOMMutationObserver* aObserver)
  {
    nsAnimationReceiver* r = new nsAnimationReceiver(aTarget, aObserver);
    r->AddObserver();
    return r;
  }

  static nsAnimationReceiver* Create(nsINode* aRegisterTarget,
                                     nsMutationReceiverBase* aParent)
  {
    nsAnimationReceiver* r = new nsAnimationReceiver(aRegisterTarget, aParent);
    aParent->AddClone(r);
    r->AddObserver();
    return r;
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONADDED
  NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONCHANGED
  NS_DECL_NSIANIMATIONOBSERVER_ANIMATIONREMOVED

protected:
  virtual ~nsAnimationReceiver() {}

  nsAnimationReceiver(nsINode* aTarget, nsDOMMutationObserver* aObserver)
    : nsMutationReceiver(aTarget, aObserver) {}

  nsAnimationReceiver(nsINode* aRegisterTarget, nsMutationReceiverBase* aParent)
    : nsMutationReceiver(aRegisterTarget, aParent) {}

  virtual void AddMutationObserver() override
  {
    mRegisterTarget->AddAnimationObserver(this);
  }

private:
  enum AnimationMutation {
    eAnimationMutation_Added,
    eAnimationMutation_Changed,
    eAnimationMutation_Removed
  };

  void RecordAnimationMutation(mozilla::dom::Animation* aAnimation,
                               AnimationMutation aMutationType);
};

#define NS_DOM_MUTATION_OBSERVER_IID \
{ 0x0c3b91f8, 0xcc3b, 0x4b08, \
  { 0x9e, 0xab, 0x07, 0x47, 0xa9, 0xe4, 0x65, 0xb4 } }

class nsDOMMutationObserver final : public nsISupports,
                                    public nsWrapperCache
{
public:
  nsDOMMutationObserver(already_AddRefed<nsPIDOMWindowInner>&& aOwner,
                        mozilla::dom::MutationCallback& aCb,
                        bool aChrome)
  : mOwner(aOwner), mLastPendingMutation(nullptr), mPendingMutationCount(0),
    mCallback(&aCb), mWaitingForRun(false), mIsChrome(aChrome),
    mMergeAttributeRecords(false), mId(++sCount)
  {
  }
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMMutationObserver)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOM_MUTATION_OBSERVER_IID)

  static already_AddRefed<nsDOMMutationObserver>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              mozilla::dom::MutationCallback& aCb,
              mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::MutationObserverBinding::Wrap(aCx, this, aGivenProto);
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  bool IsChrome()
  {
    return mIsChrome;
  }

  void Observe(nsINode& aTarget,
               const mozilla::dom::MutationObserverInit& aOptions,
               mozilla::ErrorResult& aRv);

  void Disconnect();

  void TakeRecords(nsTArray<RefPtr<nsDOMMutationRecord> >& aRetVal);

  void HandleMutation();

  void GetObservingInfo(nsTArray<Nullable<MutationObservingInfo>>& aResult,
                        mozilla::ErrorResult& aRv);

  mozilla::dom::MutationCallback* MutationCallback() { return mCallback; }

  bool MergeAttributeRecords()
  {
    return mMergeAttributeRecords;
  }

  void SetMergeAttributeRecords(bool aVal)
  {
    mMergeAttributeRecords = aVal;
  }

  // If both records are for 'attributes' type and for the same target and
  // attribute name and namespace are the same, we can skip the newer record.
  // aOldRecord->mPrevValue holds the original value, if observed.
  bool MergeableAttributeRecord(nsDOMMutationRecord* aOldRecord,
                                nsDOMMutationRecord* aRecord);

  void AppendMutationRecord(already_AddRefed<nsDOMMutationRecord> aRecord)
  {
    RefPtr<nsDOMMutationRecord> record = aRecord;
    MOZ_ASSERT(record);
    if (!mLastPendingMutation) {
      MOZ_ASSERT(!mFirstPendingMutation);
      mFirstPendingMutation = record.forget();
      mLastPendingMutation = mFirstPendingMutation;
    } else {
      MOZ_ASSERT(mFirstPendingMutation);
      mLastPendingMutation->mNext = record.forget();
      mLastPendingMutation = mLastPendingMutation->mNext;
    }
    ++mPendingMutationCount;
  }

  void ClearPendingRecords()
  {
    mFirstPendingMutation = nullptr;
    mLastPendingMutation = nullptr;
    mPendingMutationCount = 0;
  }

  // static methods
  static void QueueMutationObserverMicroTask();

  static void HandleMutations(mozilla::AutoSlowOperation& aAso);

  static bool AllScheduledMutationObserversAreSuppressed()
  {
    if (sScheduledMutationObservers) {
      uint32_t len = sScheduledMutationObservers->Length();
      if (len > 0) {
        for (uint32_t i = 0; i < len; ++i) {
          if (!(*sScheduledMutationObservers)[i]->Suppressed()) {
            return false;
          }
        }
        return true;
      }
    }
    return false;
  }

  static void EnterMutationHandling();
  static void LeaveMutationHandling();

  static void Shutdown();
protected:
  virtual ~nsDOMMutationObserver();

  friend class nsMutationReceiver;
  friend class nsAnimationReceiver;
  friend class nsAutoMutationBatch;
  friend class nsAutoAnimationMutationBatch;
  nsMutationReceiver* GetReceiverFor(nsINode* aNode,
                                     bool aMayCreate,
                                     bool aWantsAnimations);
  void RemoveReceiver(nsMutationReceiver* aReceiver);

  already_AddRefed<nsIVariant> TakeRecords();

  void GetAllSubtreeObserversFor(nsINode* aNode,
                                 nsTArray<nsMutationReceiver*>& aObservers);
  void ScheduleForRun();
  void RescheduleForRun();

  nsDOMMutationRecord* CurrentRecord(nsAtom* aType);
  bool HasCurrentRecord(const nsAString& aType);

  bool Suppressed()
  {
    return mOwner && nsGlobalWindowInner::Cast(mOwner)->IsInSyncOperation();
  }

  static void HandleMutationsInternal(mozilla::AutoSlowOperation& aAso);

  static void AddCurrentlyHandlingObserver(nsDOMMutationObserver* aObserver,
                                           uint32_t aMutationLevel);

  nsCOMPtr<nsPIDOMWindowInner>                       mOwner;

  nsCOMArray<nsMutationReceiver>                     mReceivers;
  nsClassHashtable<nsISupportsHashKey,
                   nsCOMArray<nsMutationReceiver> >  mTransientReceivers;
  // MutationRecords which are being constructed.
  AutoTArray<nsDOMMutationRecord*, 4>              mCurrentMutations;
  // MutationRecords which will be handed to the callback at the end of
  // the microtask.
  RefPtr<nsDOMMutationRecord>                      mFirstPendingMutation;
  nsDOMMutationRecord*                               mLastPendingMutation;
  uint32_t                                           mPendingMutationCount;

  RefPtr<mozilla::dom::MutationCallback>           mCallback;

  bool                                               mWaitingForRun;
  bool                                               mIsChrome;
  bool                                               mMergeAttributeRecords;

  uint64_t                                           mId;

  static uint64_t                                    sCount;
  static AutoTArray<RefPtr<nsDOMMutationObserver>, 4>* sScheduledMutationObservers;

  static uint32_t                                    sMutationLevel;
  static AutoTArray<AutoTArray<RefPtr<nsDOMMutationObserver>, 4>, 4>*
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
  AutoTArray<BatchObserver, 2> mObservers;
  nsTArray<nsCOMPtr<nsIContent> > mRemovedNodes;
  nsTArray<nsCOMPtr<nsIContent> > mAddedNodes;
  nsINode* mBatchTarget;
  bool mRemovalDone;
  bool mFromFirstToLast;
  bool mAllowNestedBatches;
  nsCOMPtr<nsINode> mPrevSibling;
  nsCOMPtr<nsINode> mNextSibling;
};

class nsAutoAnimationMutationBatch
{
  struct Entry;

public:
  explicit nsAutoAnimationMutationBatch(nsIDocument* aDocument)
  {
    Init(aDocument);
  }

  void Init(nsIDocument* aDocument)
  {
    if (!aDocument ||
        !aDocument->MayHaveDOMMutationObservers() ||
        sCurrentBatch) {
      return;
    }

    sCurrentBatch = this;
    nsDOMMutationObserver::EnterMutationHandling();
  }

  ~nsAutoAnimationMutationBatch()
  {
    Done();
  }

  void Done();

  static bool IsBatching()
  {
    return !!sCurrentBatch;
  }

  static nsAutoAnimationMutationBatch* GetCurrentBatch()
  {
    return sCurrentBatch;
  }

  static void AddObserver(nsDOMMutationObserver* aObserver)
  {
    if (sCurrentBatch->mObservers.Contains(aObserver)) {
      return;
    }
    sCurrentBatch->mObservers.AppendElement(aObserver);
  }

  static void AnimationAdded(mozilla::dom::Animation* aAnimation,
                             nsINode* aTarget)
  {
    if (!IsBatching()) {
      return;
    }

    Entry* entry = sCurrentBatch->FindEntry(aAnimation, aTarget);
    if (entry) {
      switch (entry->mState) {
        case eState_RemainedAbsent:
          entry->mState = eState_Added;
          break;
        case eState_Removed:
          entry->mState = eState_RemainedPresent;
          break;
        default:
          NS_NOTREACHED("shouldn't have observed an animation being added "
                        "twice");
      }
    } else {
      entry = sCurrentBatch->AddEntry(aAnimation, aTarget);
      entry->mState = eState_Added;
      entry->mChanged = false;
    }
  }

  static void AnimationChanged(mozilla::dom::Animation* aAnimation,
                               nsINode* aTarget)
  {
    Entry* entry = sCurrentBatch->FindEntry(aAnimation, aTarget);
    if (entry) {
      NS_ASSERTION(entry->mState == eState_RemainedPresent ||
                   entry->mState == eState_Added,
                   "shouldn't have observed an animation being changed after "
                   "being removed");
      entry->mChanged = true;
    } else {
      entry = sCurrentBatch->AddEntry(aAnimation, aTarget);
      entry->mState = eState_RemainedPresent;
      entry->mChanged = true;
    }
  }

  static void AnimationRemoved(mozilla::dom::Animation* aAnimation,
                               nsINode* aTarget)
  {
    Entry* entry = sCurrentBatch->FindEntry(aAnimation, aTarget);
    if (entry) {
      switch (entry->mState) {
        case eState_RemainedPresent:
          entry->mState = eState_Removed;
          break;
        case eState_Added:
          entry->mState = eState_RemainedAbsent;
          break;
        default:
          NS_NOTREACHED("shouldn't have observed an animation being removed "
                        "twice");
      }
    } else {
      entry = sCurrentBatch->AddEntry(aAnimation, aTarget);
      entry->mState = eState_Removed;
      entry->mChanged = false;
    }
  }

private:
  Entry* FindEntry(mozilla::dom::Animation* aAnimation, nsINode* aTarget)
  {
    EntryArray* entries = mEntryTable.Get(aTarget);
    if (!entries) {
      return nullptr;
    }

    for (Entry& e : *entries) {
      if (e.mAnimation == aAnimation) {
        return &e;
      }
    }
    return nullptr;
  }

  Entry* AddEntry(mozilla::dom::Animation* aAnimation, nsINode* aTarget)
  {
    EntryArray* entries = sCurrentBatch->mEntryTable.LookupOrAdd(aTarget);
    if (entries->IsEmpty()) {
      sCurrentBatch->mBatchTargets.AppendElement(aTarget);
    }
    Entry* entry = entries->AppendElement();
    entry->mAnimation = aAnimation;
    return entry;
  }

  enum State {
    eState_RemainedPresent,
    eState_RemainedAbsent,
    eState_Added,
    eState_Removed
  };

  struct Entry
  {
    RefPtr<mozilla::dom::Animation> mAnimation;
    State mState;
    bool mChanged;
  };

  static nsAutoAnimationMutationBatch* sCurrentBatch;
  AutoTArray<nsDOMMutationObserver*, 2> mObservers;
  typedef nsTArray<Entry> EntryArray;
  nsClassHashtable<nsPtrHashKey<nsINode>, EntryArray> mEntryTable;
  // List of nodes referred to by mEntryTable so we can sort them
  // For a specific pseudo element, we use its parent element as the
  // batch target, so they will be put in the same EntryArray.
  nsTArray<nsINode*> mBatchTargets;
};

inline
nsDOMMutationObserver*
nsMutationReceiverBase::Observer()
{
  return mParent ?
    mParent->Observer() : static_cast<nsDOMMutationObserver*>(mObserver);
}

#endif
