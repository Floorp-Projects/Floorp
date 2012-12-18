/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMutationObserver.h"        
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsIDOMMutationEvent.h"
#include "nsTextFragment.h"
#include "jsapi.h"
#include "nsServiceManagerUtils.h"

nsTArray<nsRefPtr<nsDOMMutationObserver> >*
  nsDOMMutationObserver::sScheduledMutationObservers = nullptr;

nsDOMMutationObserver* nsDOMMutationObserver::sCurrentObserver = nullptr;

uint32_t nsDOMMutationObserver::sMutationLevel = 0;
uint64_t nsDOMMutationObserver::sCount = 0;

nsAutoTArray<nsTArray<nsRefPtr<nsDOMMutationObserver> >, 4>*
nsDOMMutationObserver::sCurrentlyHandlingObservers = nullptr;

nsINodeList*
nsDOMMutationRecord::AddedNodes()
{
  if (!mAddedNodes) {
    mAddedNodes = new nsSimpleContentList(mTarget);
  }
  return mAddedNodes;
}

nsINodeList*
nsDOMMutationRecord::RemovedNodes()
{
  if (!mRemovedNodes) {
    mRemovedNodes = new nsSimpleContentList(mTarget);
  }
  return mRemovedNodes;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMutationRecord)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMMutationRecord)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMMutationRecord)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMMutationRecord)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMMutationRecord)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMutationRecord)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPreviousSibling)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNextSibling)
  tmp->mAddedNodes = nullptr;
  tmp->mRemovedNodes = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMutationRecord)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPreviousSibling)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNextSibling)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAddedNodes");
  cb.NoteXPCOMChild(static_cast<nsIDOMNodeList*>(tmp->mAddedNodes));
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRemovedNodes");
  cb.NoteXPCOMChild(static_cast<nsIDOMNodeList*>(tmp->mRemovedNodes));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// Observer

NS_IMPL_ADDREF(nsMutationReceiver)
NS_IMPL_RELEASE(nsMutationReceiver)

NS_INTERFACE_MAP_BEGIN(nsMutationReceiver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsMutationReceiver)
NS_INTERFACE_MAP_END

nsMutationReceiver::nsMutationReceiver(nsINode* aTarget,
                                       nsDOMMutationObserver* aObserver)
: nsMutationReceiverBase(aTarget, aObserver)
{
  mTarget->BindObject(aObserver);
}

void
nsMutationReceiver::Disconnect(bool aRemoveFromObserver)
{
  if (mRegisterTarget) {
    mRegisterTarget->RemoveMutationObserver(this);
    mRegisterTarget = nullptr;
  }

  mParent = nullptr;
  nsINode* target = mTarget;
  mTarget = nullptr;
  nsDOMMutationObserver* observer = mObserver;
  mObserver = nullptr;
  RemoveClones();

  if (target && observer) {
    if (aRemoveFromObserver) {
      static_cast<nsDOMMutationObserver*>(observer)->RemoveReceiver(this);
    }
    // UnbindObject may delete 'this'!
    target->UnbindObject(observer);
  }
}

void
nsMutationReceiver::AttributeWillChange(nsIDocument* aDocument,
                                        mozilla::dom::Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        int32_t aModType)
{
  if (nsAutoMutationBatch::IsBatching() ||
      !ObservesAttr(aElement, aNameSpaceID, aAttribute) ||
      aElement->ChromeOnlyAccess()) {
    return;
  }

  nsDOMMutationRecord* m =
    Observer()->CurrentRecord(NS_LITERAL_STRING("attributes"));

  NS_ASSERTION(!m->mTarget || m->mTarget == aElement,
               "Wrong target!");
  NS_ASSERTION(m->mAttrName.IsVoid() ||
               m->mAttrName.Equals(nsDependentAtomString(aAttribute)),
               "Wrong attribute!");
  if (!m->mTarget) {
    m->mTarget = aElement;
    m->mAttrName = nsAtomString(aAttribute);
    if (aNameSpaceID == kNameSpaceID_None) {
      m->mAttrNamespace.SetIsVoid(true);
    } else {
      nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID,
                                                          m->mAttrNamespace);
    }
  }

  if (AttributeOldValue() && m->mPrevValue.IsVoid()) {
    if (!aElement->GetAttr(aNameSpaceID, aAttribute, m->mPrevValue)) {
      m->mPrevValue.SetIsVoid(true);
    }
  }
}

void
nsMutationReceiver::CharacterDataWillChange(nsIDocument *aDocument,
                                            nsIContent* aContent,
                                            CharacterDataChangeInfo* aInfo)
{
  if (nsAutoMutationBatch::IsBatching() ||
      !CharacterData() || !(Subtree() || aContent == Target()) ||
      aContent->ChromeOnlyAccess()) {
    return;
  }
  
  nsDOMMutationRecord* m =
    Observer()->CurrentRecord(NS_LITERAL_STRING("characterData"));
 
  NS_ASSERTION(!m->mTarget || m->mTarget == aContent,
               "Wrong target!");

  if (!m->mTarget) {
    m->mTarget = aContent;
  }
  if (CharacterDataOldValue() && m->mPrevValue.IsVoid()) { 
    aContent->GetText()->AppendTo(m->mPrevValue);
  }
}

void
nsMutationReceiver::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    int32_t aNewIndexInContainer)
{
  nsINode* parent = NODE_FROM(aContainer, aDocument);
  bool wantsChildList = ChildList() && (Subtree() || parent == Target());
  if (!wantsChildList || aFirstNewContent->ChromeOnlyAccess()) {
    return;
  }

  if (nsAutoMutationBatch::IsBatching()) {
    if (parent == nsAutoMutationBatch::GetBatchTarget()) {
      nsAutoMutationBatch::UpdateObserver(Observer(), wantsChildList);
    }
    return;
  }

  nsDOMMutationRecord* m =
    Observer()->CurrentRecord(NS_LITERAL_STRING("childList"));
  NS_ASSERTION(!m->mTarget || m->mTarget == parent,
               "Wrong target!");
  if (m->mTarget) {
    // Already handled case.
    return;
  }
  m->mTarget = parent;
  m->mAddedNodes = new nsSimpleContentList(parent);

  nsINode* n = aFirstNewContent;
  while (n) {
    m->mAddedNodes->AppendElement(static_cast<nsIContent*>(n));
    n = n->GetNextSibling();
  }
  m->mPreviousSibling = aFirstNewContent->GetPreviousSibling();
}

void
nsMutationReceiver::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    int32_t aIndexInContainer)
{
  nsINode* parent = NODE_FROM(aContainer, aDocument);
  bool wantsChildList = ChildList() && (Subtree() || parent == Target());
  if (!wantsChildList || aChild->ChromeOnlyAccess()) {
    return;
  }

  if (nsAutoMutationBatch::IsBatching()) {
    if (parent == nsAutoMutationBatch::GetBatchTarget()) {
      nsAutoMutationBatch::UpdateObserver(Observer(), wantsChildList);
    }
    return;
  }

  nsDOMMutationRecord* m =
    Observer()->CurrentRecord(NS_LITERAL_STRING("childList"));
  if (m->mTarget) {
    // Already handled case.
    return;  
  }
  m->mTarget = parent;
  m->mAddedNodes = new nsSimpleContentList(parent);
  m->mAddedNodes->AppendElement(aChild);
  m->mPreviousSibling = aChild->GetPreviousSibling();
  m->mNextSibling = aChild->GetNextSibling();
}

void
nsMutationReceiver::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   int32_t aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  if (aChild->ChromeOnlyAccess()) {
    return;
  }

  nsINode* parent = NODE_FROM(aContainer, aDocument);
  if (nsAutoMutationBatch::IsBatching()) {
    if (nsAutoMutationBatch::IsRemovalDone()) {
      // This can happen for example if HTML parser parses to
      // context node, but needs to move elements around.
      return;
    }
    if (nsAutoMutationBatch::GetBatchTarget() != parent) {
      return;
    }

    bool wantsChildList = ChildList() && (Subtree() || parent == Target());
    if (wantsChildList || Subtree()) {
      nsAutoMutationBatch::NodeRemoved(aChild);
      nsAutoMutationBatch::UpdateObserver(Observer(), wantsChildList);
    }

    return;
  }                                                                   

  if (Subtree()) {
    // Try to avoid creating transient observer if the node
    // already has an observer observing the same set of nodes.
    nsMutationReceiver* orig = GetParent() ? GetParent() : this;
    if (Observer()->GetReceiverFor(aChild, false) != orig) {
      bool transientExists = false;
      nsCOMArray<nsMutationReceiver>* transientReceivers = nullptr;
      Observer()->mTransientReceivers.Get(aChild, &transientReceivers);
      if (!transientReceivers) {
        transientReceivers = new nsCOMArray<nsMutationReceiver>();
        Observer()->mTransientReceivers.Put(aChild, transientReceivers);
      } else {
        for (int32_t i = 0; i < transientReceivers->Count(); ++i) {
          nsMutationReceiver* r = transientReceivers->ObjectAt(i);
          if (r->GetParent() == orig) {
            transientExists = true;
          }
        }
      }
      if (!transientExists) {
        // Make sure the elements which are removed from the
        // subtree are kept in the same observation set.
        transientReceivers->AppendObject(new nsMutationReceiver(aChild, orig));
      }
    }
  }

  if (ChildList() && (Subtree() || parent == Target())) {
    nsDOMMutationRecord* m =
      Observer()->CurrentRecord(NS_LITERAL_STRING("childList"));
    if (m->mTarget) {
      // Already handled case.
      return;
    }
    m->mTarget = parent;
    m->mRemovedNodes = new nsSimpleContentList(parent);
    m->mRemovedNodes->AppendElement(aChild);
    m->mPreviousSibling = aPreviousSibling;
    m->mNextSibling = parent->GetChildAt(aIndexInContainer);
  }
  // We need to schedule always, so that after microtask mTransientReceivers
  // can be cleared correctly.
  Observer()->ScheduleForRun();
}

void nsMutationReceiver::NodeWillBeDestroyed(const nsINode *aNode)
{
  NS_ASSERTION(!mParent, "Shouldn't have mParent here!");
  Disconnect(true);
}

// Observer

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMutationObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMMutationObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMMutationObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMMutationObserver)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMMutationObserver)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMutationObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  for (int32_t i = 0; i < tmp->mReceivers.Count(); ++i) {
    tmp->mReceivers[i]->Disconnect(false);
  }
  tmp->mReceivers.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingMutations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  // No need to handle mTransientReceivers
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMutationObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReceivers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingMutations)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  // No need to handle mTransientReceivers
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsMutationReceiver*
nsDOMMutationObserver::GetReceiverFor(nsINode* aNode, bool aMayCreate)
{
  if (!aMayCreate && !aNode->MayHaveDOMMutationObserver()) {
    return nullptr;
  }

  for (int32_t i = 0; i < mReceivers.Count(); ++i) {
    if (mReceivers[i]->Target() == aNode) {
      return mReceivers[i];
    }
  }
  if (!aMayCreate) {
    return nullptr;
  }

  nsMutationReceiver* r = new nsMutationReceiver(aNode, this);
  mReceivers.AppendObject(r);
  return r;
}

void
nsDOMMutationObserver::RemoveReceiver(nsMutationReceiver* aReceiver)
{
  mReceivers.RemoveObject(aReceiver);
}

void
nsDOMMutationObserver::GetAllSubtreeObserversFor(nsINode* aNode,
                                                 nsTArray<nsMutationReceiver*>&
                                                   aReceivers)
{
  nsINode* n = aNode;
  while (n) {
    if (n->MayHaveDOMMutationObserver()) {
      nsMutationReceiver* r = GetReceiverFor(n, false);
      if (r && r->Subtree() && !aReceivers.Contains(r)) {
        aReceivers.AppendElement(r);
        // If we've found all the receivers the observer has,
        // no need to search for more.
        if (mReceivers.Count() == int32_t(aReceivers.Length())) {
          return;
        }
      }                                            
      nsCOMArray<nsMutationReceiver>* transientReceivers = nullptr;
      if (mTransientReceivers.Get(n, &transientReceivers) && transientReceivers) {
        for (int32_t i = 0; i < transientReceivers->Count(); ++i) {
          nsMutationReceiver* r = transientReceivers->ObjectAt(i);
          nsMutationReceiver* parent = r->GetParent();
          if (r->Subtree() && parent && !aReceivers.Contains(parent)) {
            aReceivers.AppendElement(parent);
          }
        }
        if (mReceivers.Count() == int32_t(aReceivers.Length())) {
          return;
        }
      }
    }
    n = n->GetParentNode();
  }
}

void
nsDOMMutationObserver::ScheduleForRun()
{
  nsDOMMutationObserver::AddCurrentlyHandlingObserver(this);

  if (mWaitingForRun) {
    return;
  }
  mWaitingForRun = true;
  RescheduleForRun();
}

void
nsDOMMutationObserver::RescheduleForRun()
{
  if (!sScheduledMutationObservers) {
    sScheduledMutationObservers = new nsTArray<nsRefPtr<nsDOMMutationObserver> >;
  }

  bool didInsert = false;
  for (uint32_t i = 0; i < sScheduledMutationObservers->Length(); ++i) {
    if (static_cast<nsDOMMutationObserver*>((*sScheduledMutationObservers)[i])
          ->mId > mId) {
      sScheduledMutationObservers->InsertElementAt(i, this);
      didInsert = true;
      break;
    }
  }
  if (!didInsert) {
    sScheduledMutationObservers->AppendElement(this);
  }
}

void
nsDOMMutationObserver::Observe(nsINode& aTarget,
                               const mozilla::dom::MutationObserverInit& aOptions,
                               mozilla::ErrorResult& aRv)
{

  if (!(aOptions.childList || aOptions.attributes || aOptions.characterData)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  if (aOptions.attributeOldValue && !aOptions.attributes) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }
  if (aOptions.characterDataOldValue && !aOptions.characterData) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nsCOMArray<nsIAtom> filters;
  bool allAttrs = true;
  if (aOptions.attributeFilter.WasPassed()) {
    allAttrs = false;
    const mozilla::dom::Sequence<nsString>& filtersAsString =
      aOptions.attributeFilter.Value();
    uint32_t len = filtersAsString.Length();

    if (len != 0 && !aOptions.attributes) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }
    if (!filters.SetCapacity(len)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    for (uint32_t i = 0; i < len; ++i) {
      nsCOMPtr<nsIAtom> a = do_GetAtom(filtersAsString[i]);
      filters.AppendObject(a);
    }
  }

  nsMutationReceiver* r = GetReceiverFor(&aTarget, true);
  r->SetChildList(aOptions.childList);
  r->SetAttributes(aOptions.attributes);
  r->SetCharacterData(aOptions.characterData);
  r->SetSubtree(aOptions.subtree);
  r->SetAttributeOldValue(aOptions.attributeOldValue);
  r->SetCharacterDataOldValue(aOptions.characterDataOldValue);
  r->SetAttributeFilter(filters);
  r->SetAllAttributes(allAttrs);
  r->RemoveClones();

#ifdef DEBUG
  for (int32_t i = 0; i < mReceivers.Count(); ++i) {
    NS_WARN_IF_FALSE(mReceivers[i]->Target(),
                     "All the receivers should have a target!");
  }
#endif
}

void
nsDOMMutationObserver::Disconnect()
{
  for (int32_t i = 0; i < mReceivers.Count(); ++i) {
    mReceivers[i]->Disconnect(false);
  }
  mReceivers.Clear();
  mCurrentMutations.Clear();
  mPendingMutations.Clear();
}

void
nsDOMMutationObserver::TakeRecords(
                         nsTArray<nsRefPtr<nsDOMMutationRecord> >& aRetVal)
{
  aRetVal.Clear();
  mPendingMutations.SwapElements(aRetVal);
}

// static
already_AddRefed<nsDOMMutationObserver>
nsDOMMutationObserver::Constructor(nsISupports* aGlobal,
                                   mozilla::dom::MutationCallback& aCb,
                                   mozilla::ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal);
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  MOZ_ASSERT(window->IsInnerWindow());
  nsRefPtr<nsDOMMutationObserver> observer =
    new nsDOMMutationObserver(window.forget(), aCb);
  return observer.forget();
}

void
nsDOMMutationObserver::HandleMutation()
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "Whaat!");
  NS_ASSERTION(mCurrentMutations.IsEmpty(),
               "Still generating MutationRecords?");

  mWaitingForRun = false;

  for (int32_t i = 0; i < mReceivers.Count(); ++i) {
    mReceivers[i]->RemoveClones();
  }
  mTransientReceivers.Clear();

  nsPIDOMWindow* outer = mOwner->GetOuterWindow();
  if (!mPendingMutations.Length() || !outer ||
      outer->GetCurrentInnerWindow() != mOwner) {
    mPendingMutations.Clear();
    return;
  }

  nsTArray<nsRefPtr<nsDOMMutationRecord> > mutationsArray;
  TakeRecords(mutationsArray);
  mozilla::dom::Sequence<mozilla::dom::OwningNonNull<nsDOMMutationRecord> >
    mutations;
  uint32_t len = mutationsArray.Length();
  NS_ENSURE_TRUE_VOID(mutations.SetCapacity(len));
  for (uint32_t i = 0; i < len; ++i) {
    *mutations.AppendElement() = mutationsArray[i].forget();
  }
  mozilla::ErrorResult rv;
  mCallback->Call(this, mutations, *this, rv);
}

class AsyncMutationHandler : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    nsDOMMutationObserver::HandleMutations();
    return NS_OK;
  }
};

void
nsDOMMutationObserver::HandleMutationsInternal()
{
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::AddScriptRunner(new AsyncMutationHandler());
    return;
  }
  static nsRefPtr<nsDOMMutationObserver> sCurrentObserver;
  if (sCurrentObserver && !sCurrentObserver->Suppressed()) {
    // In normal cases sScheduledMutationObservers will be handled
    // after previous mutations are handled. But in case some
    // callback calls a sync API, which spins the eventloop, we need to still
    // process other mutations happening during that sync call.
    // This does *not* catch all cases, but should work for stuff running
    // in separate tabs.
    return;
  }

  nsTArray<nsRefPtr<nsDOMMutationObserver> >* suppressedObservers = nullptr;

  while (sScheduledMutationObservers) {
    nsTArray<nsRefPtr<nsDOMMutationObserver> >* observers = sScheduledMutationObservers;
    sScheduledMutationObservers = nullptr;
    for (uint32_t i = 0; i < observers->Length(); ++i) {
      sCurrentObserver = static_cast<nsDOMMutationObserver*>((*observers)[i]);
      if (!sCurrentObserver->Suppressed()) {
        sCurrentObserver->HandleMutation();
      } else {
        if (!suppressedObservers) {
          suppressedObservers = new nsTArray<nsRefPtr<nsDOMMutationObserver> >;
        }
        if (!suppressedObservers->Contains(sCurrentObserver)) {
          suppressedObservers->AppendElement(sCurrentObserver);
        }
      }
    }
    delete observers;
  }

  if (suppressedObservers) {
    for (uint32_t i = 0; i < suppressedObservers->Length(); ++i) {
      static_cast<nsDOMMutationObserver*>(suppressedObservers->ElementAt(i))->
        RescheduleForRun();
    }
    delete suppressedObservers;
    suppressedObservers = nullptr;
  }
  sCurrentObserver = nullptr;
}

nsDOMMutationRecord*
nsDOMMutationObserver::CurrentRecord(const nsAString& aType)
{
  NS_ASSERTION(sMutationLevel > 0, "Unexpected mutation level!");

  while (mCurrentMutations.Length() < sMutationLevel) {
    mCurrentMutations.AppendElement(static_cast<nsDOMMutationRecord*>(nullptr));
  }

  uint32_t last = sMutationLevel - 1;
  if (!mCurrentMutations[last]) {
    nsDOMMutationRecord* r = new nsDOMMutationRecord(aType, GetParentObject());
    mCurrentMutations[last] = r;
    mPendingMutations.AppendElement(r);
    ScheduleForRun();
  }

  NS_ASSERTION(mCurrentMutations[last]->mType.Equals(aType),
               "Unexpected MutationRecord type!");

  return mCurrentMutations[last];
}

nsDOMMutationObserver::~nsDOMMutationObserver()
{
  for (int32_t i = 0; i < mReceivers.Count(); ++i) {
    mReceivers[i]->RemoveClones();
  }
}

void
nsDOMMutationObserver::EnterMutationHandling()
{
  ++sMutationLevel;
}

// Leave the current mutation level (there can be several levels if in case
// of nested calls to the nsIMutationObserver methods).
// The most recent mutation record is removed from mCurrentMutations, so
// that is doesn't get modified anymore by receivers.
void
nsDOMMutationObserver::LeaveMutationHandling()
{
  if (sCurrentlyHandlingObservers &&
      sCurrentlyHandlingObservers->Length() == sMutationLevel) {
    nsTArray<nsRefPtr<nsDOMMutationObserver> >& obs =
      sCurrentlyHandlingObservers->ElementAt(sMutationLevel - 1);
    for (uint32_t i = 0; i < obs.Length(); ++i) {
      nsDOMMutationObserver* o =
        static_cast<nsDOMMutationObserver*>(obs[i]);
      if (o->mCurrentMutations.Length() == sMutationLevel) {
        // It is already in pending mutations.
        o->mCurrentMutations.RemoveElementAt(sMutationLevel - 1);
      }
    }
    sCurrentlyHandlingObservers->RemoveElementAt(sMutationLevel - 1);
  }
  --sMutationLevel;
}

void
nsDOMMutationObserver::AddCurrentlyHandlingObserver(nsDOMMutationObserver* aObserver)
{
  NS_ASSERTION(sMutationLevel > 0, "Unexpected mutation level!");

  if (!sCurrentlyHandlingObservers) {
    sCurrentlyHandlingObservers =
      new nsAutoTArray<nsTArray<nsRefPtr<nsDOMMutationObserver> >, 4>;
  }

  while (sCurrentlyHandlingObservers->Length() < sMutationLevel) {
    sCurrentlyHandlingObservers->InsertElementAt(
      sCurrentlyHandlingObservers->Length());
  }

  uint32_t last = sMutationLevel - 1;
  if (!sCurrentlyHandlingObservers->ElementAt(last).Contains(aObserver)) {
    sCurrentlyHandlingObservers->ElementAt(last).AppendElement(aObserver);
  }
}

void
nsDOMMutationObserver::Shutdown()
{
  delete sCurrentlyHandlingObservers;
  sCurrentlyHandlingObservers = nullptr;
  delete sScheduledMutationObservers;
  sScheduledMutationObservers = nullptr;
}

nsAutoMutationBatch*
nsAutoMutationBatch::sCurrentBatch = nullptr;

void
nsAutoMutationBatch::Done()
{
  if (sCurrentBatch != this) {
    return;
  }

  sCurrentBatch = mPreviousBatch;
  if (mObservers.IsEmpty()) {
    nsDOMMutationObserver::LeaveMutationHandling();
    // Nothing to do.
    return;
  }

  uint32_t len = mObservers.Length();
  for (uint32_t i = 0; i < len; ++i) {
    nsDOMMutationObserver* ob = mObservers[i].mObserver;
    bool wantsChildList = mObservers[i].mWantsChildList;

    nsRefPtr<nsSimpleContentList> removedList;
    if (wantsChildList) {
      removedList = new nsSimpleContentList(mBatchTarget);
    }

    nsTArray<nsMutationReceiver*> allObservers;
    ob->GetAllSubtreeObserversFor(mBatchTarget, allObservers);

    int32_t j = mFromFirstToLast ? 0 : mRemovedNodes.Length() - 1;
    int32_t end = mFromFirstToLast ? mRemovedNodes.Length() : -1;
    for (; j != end; mFromFirstToLast ? ++j : --j) {
      nsCOMPtr<nsIContent> removed = mRemovedNodes[j];
      if (removedList) {
        removedList->AppendElement(removed);
      }

      if (allObservers.Length()) {
        nsCOMArray<nsMutationReceiver>* transientReceivers = nullptr;
        ob->mTransientReceivers.Get(removed, &transientReceivers);
        if (!transientReceivers) {
          transientReceivers = new nsCOMArray<nsMutationReceiver>();
          ob->mTransientReceivers.Put(removed, transientReceivers);
        }
        for (uint32_t k = 0; k < allObservers.Length(); ++k) {
          nsMutationReceiver* r = allObservers[k];
          nsMutationReceiver* orig = r->GetParent() ? r->GetParent() : r;
          if (ob->GetReceiverFor(removed, false) != orig) {
            // Make sure the elements which are removed from the
            // subtree are kept in the same observation set.
            transientReceivers->AppendObject(new nsMutationReceiver(removed, orig));
          }
        }
      }
    }
    if (wantsChildList && (mRemovedNodes.Length() || mAddedNodes.Length())) {
      nsRefPtr<nsSimpleContentList> addedList =
        new nsSimpleContentList(mBatchTarget);
      for (uint32_t i = 0; i < mAddedNodes.Length(); ++i) {
        addedList->AppendElement(mAddedNodes[i]);
      }
      nsDOMMutationRecord* m =
        new nsDOMMutationRecord(NS_LITERAL_STRING("childList"),
                                ob->GetParentObject());
      ob->mPendingMutations.AppendElement(m);
      m->mTarget = mBatchTarget;
      m->mRemovedNodes = removedList;
      m->mAddedNodes = addedList;
      m->mPreviousSibling = mPrevSibling;
      m->mNextSibling = mNextSibling;
    }
    // Always schedule the observer so that transient receivers are
    // removed correctly.
    ob->ScheduleForRun();
  }
  nsDOMMutationObserver::LeaveMutationHandling();
}
