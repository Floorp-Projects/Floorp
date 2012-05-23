/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMutationObserver_h
#define nsDOMMutationObserver_h

#include "nsIDOMMutationObserver.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIJSNativeInitializer.h"
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

class nsDOMMutationObserver;

class nsDOMMutationRecord : public nsIDOMMutationRecord
{
public:
  nsDOMMutationRecord(const nsAString& aType) : mType(aType)
  {
    mAttrName.SetIsVoid(PR_TRUE);
    mAttrNamespace.SetIsVoid(PR_TRUE);
    mPrevValue.SetIsVoid(PR_TRUE);
  }
  virtual ~nsDOMMutationRecord() {}
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMMutationRecord)
  NS_DECL_NSIDOMMUTATIONRECORD

  nsCOMPtr<nsINode>             mTarget;
  nsString                      mType;
  nsString                      mAttrName;
  nsString                      mAttrNamespace;
  nsString                      mPrevValue;
  nsRefPtr<nsSimpleContentList> mAddedNodes;
  nsRefPtr<nsSimpleContentList> mRemovedNodes;
  nsCOMPtr<nsINode>             mPreviousSibling;
  nsCOMPtr<nsINode>             mNextSibling;
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
  nsMutationReceiverBase(nsINode* aTarget, nsIDOMMutationObserver* aObserver)
  : mTarget(aTarget), mObserver(aObserver), mRegisterTarget(aTarget)
  {
    mRegisterTarget->AddMutationObserver(this);
    mRegisterTarget->SetMayHaveDOMMutationObserver();
    mRegisterTarget->OwnerDoc()->SetMayHaveDOMMutationObservers();
  }

  nsMutationReceiverBase(nsINode* aRegisterTarget,
                         nsMutationReceiverBase* aParent)
  : mTarget(nsnull), mObserver(nsnull), mParent(aParent),
    mRegisterTarget(aRegisterTarget), mKungFuDeathGrip(aParent->Target())
  {
    NS_ASSERTION(mParent->Subtree(), "Should clone a non-subtree observer!");
    mRegisterTarget->AddMutationObserver(this);
    mRegisterTarget->SetMayHaveDOMMutationObserver();
    mRegisterTarget->OwnerDoc()->SetMayHaveDOMMutationObservers();
  }

  bool ObservesAttr(mozilla::dom::Element* aElement,
                    PRInt32 aNameSpaceID,
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
    for (PRInt32 i = 0; i < filters.Count(); ++i) {         
      if (filters[i] == aAttr) {
        return true;
      }
    }
    return false;
  }

  // The target for the MutationObserver.observe() method.
  nsINode*                           mTarget;
  nsIDOMMutationObserver*            mObserver;
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


#define NS_MUTATION_OBSERVER_IID \
{ 0xe628f313, 0x8129, 0x4f90, \
  { 0x8e, 0xc3, 0x85, 0xe8, 0x28, 0x22, 0xe7, 0xab } }

class nsMutationReceiver : public nsMutationReceiverBase
{
public:
  nsMutationReceiver(nsINode* aTarget, nsIDOMMutationObserver* aObserver)
  : nsMutationReceiverBase(aTarget, aObserver)
  {
    mTarget->BindObject(aObserver);
  }

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
    for (PRInt32 i = 0; i < mTransientReceivers.Count(); ++i) {
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
      mRegisterTarget = nsnull;
    }

    mParent = nsnull;
    NS_ASSERTION(!mTarget, "Should not have mTarget");
    NS_ASSERTION(!mObserver, "Should not have mObserver");
  }

  void Disconnect(bool aRemoveFromObserver);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMUTATION_OBSERVER_IID)
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsMutationReceiver)

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsMutationReceiver, NS_MUTATION_OBSERVER_IID)

class nsDOMMutationObserver : public nsIDOMMutationObserver,
                              public nsIJSNativeInitializer
{
public:
  nsDOMMutationObserver() : mWaitingForRun(false), mId(++sCount)
  {
    mTransientReceivers.Init();
  }
  virtual ~nsDOMMutationObserver();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMMutationObserver,
                                           nsIDOMMutationObserver)
  NS_DECL_NSIDOMMUTATIONOBSERVER

  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                        PRUint32 argc, jsval* argv);

  void HandleMutation();

  // static methods
  static void HandleMutations()
  {
    if (sScheduledMutationObservers) {
      HandleMutationsInternal();
    }
  }

  static void EnterMutationHandling();
  static void LeaveMutationHandling();

  static nsIDOMMutationObserver* CurrentObserver()
  {
    return sCurrentObserver;
  }

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
      nsCOMPtr<nsIDocument> d = do_QueryInterface(mOwner->GetExtantDocument());
      return d && d->IsInSyncOperation();
    }
    return false;
  }

  static void HandleMutationsInternal();

  static void AddCurrentlyHandlingObserver(nsDOMMutationObserver* aObserver);

  nsCOMPtr<nsIScriptContext>                         mScriptContext;
  nsCOMPtr<nsPIDOMWindow>                            mOwner;

  nsCOMArray<nsMutationReceiver>                     mReceivers;
  nsClassHashtable<nsISupportsHashKey,
                   nsCOMArray<nsMutationReceiver> >  mTransientReceivers;  
  // MutationRecords which are being constructed.
  nsAutoTArray<nsDOMMutationRecord*, 4>              mCurrentMutations;
  // MutationRecords which will be handed to the callback at the end of
  // the microtask.
  nsCOMArray<nsDOMMutationRecord>                    mPendingMutations;
  nsCOMPtr<nsIMutationObserverCallback>              mCallback;

  bool                                               mWaitingForRun;

  PRUint64                                           mId;

  static PRUint64                                    sCount;
  static nsCOMArray<nsIDOMMutationObserver>*         sScheduledMutationObservers;
  static nsIDOMMutationObserver*                     sCurrentObserver;

  static PRUint32                                    sMutationLevel;
  static nsAutoTArray<nsCOMArray<nsIDOMMutationObserver>, 4>*
                                                     sCurrentlyHandlingObservers;
};

class nsAutoMutationBatch
{
public:
  nsAutoMutationBatch()
  : mPreviousBatch(nsnull), mBatchTarget(nsnull), mRemovalDone(false),
    mFromFirstToLast(false), mAllowNestedBatches(false)    
  {
  }

  nsAutoMutationBatch(nsINode* aTarget, bool aFromFirstToLast,
                      bool aAllowNestedBatches)
  : mPreviousBatch(nsnull), mBatchTarget(nsnull), mRemovalDone(false),
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
    PRUint32 l = sCurrentBatch->mObservers.Length();
    for (PRUint32 i = 0; i < l; ++i) {
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
      PRUint32 len = sCurrentBatch->mRemovedNodes.Length();
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

#define NS_DOMMUTATIONOBSERVER_CID           \
 { /* b66b9490-52f7-4f2a-b998-dbb1d59bc13e */ \
  0xb66b9490, 0x52f7, 0x4f2a,                 \
  { 0xb9, 0x98, 0xdb, 0xb1, 0xd5, 0x9b, 0xc1, 0x3e } }

#define NS_DOMMUTATIONOBSERVER_CONTRACTID \
  "@mozilla.org/dommutationobserver;1"

#endif
