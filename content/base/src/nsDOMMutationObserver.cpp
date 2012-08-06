/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMutationObserver.h"        
#include "nsDOMClassInfoID.h"
#include "nsDOMError.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"
#include "nsIScriptGlobalObject.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsIDOMMutationEvent.h"
#include "nsTextFragment.h"
#include "jsapi.h"
#include "nsServiceManagerUtils.h"
#include "DictionaryHelpers.h"

nsCOMArray<nsIDOMMutationObserver>*
  nsDOMMutationObserver::sScheduledMutationObservers = nullptr;

nsIDOMMutationObserver* nsDOMMutationObserver::sCurrentObserver = nullptr;

PRUint32 nsDOMMutationObserver::sMutationLevel = 0;
PRUint64 nsDOMMutationObserver::sCount = 0;

nsAutoTArray<nsCOMArray<nsIDOMMutationObserver>, 4>*
nsDOMMutationObserver::sCurrentlyHandlingObservers = nullptr;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMMutationRecord)

DOMCI_DATA(MutationRecord, nsDOMMutationRecord)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMMutationRecord)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationRecord)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MutationRecord)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMMutationRecord)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMMutationRecord)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMutationRecord)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPreviousSibling)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNextSibling)
  tmp->mAddedNodes = nullptr;
  tmp->mRemovedNodes = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMutationRecord)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPreviousSibling)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNextSibling)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAddedNodes");
  cb.NoteXPCOMChild(static_cast<nsIDOMNodeList*>(tmp->mAddedNodes));
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRemovedNodes");
  cb.NoteXPCOMChild(static_cast<nsIDOMNodeList*>(tmp->mRemovedNodes));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsDOMMutationRecord::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetTarget(nsIDOMNode** aTarget)
{
  nsCOMPtr<nsIDOMNode> n = do_QueryInterface(mTarget);
  n.forget(aTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetAddedNodes(nsIDOMNodeList** aAddedNodes)
{
  if (!mAddedNodes && mTarget) {
    mAddedNodes = new nsSimpleContentList(mTarget);
  }
  NS_IF_ADDREF(*aAddedNodes = mAddedNodes);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetRemovedNodes(nsIDOMNodeList** aRemovedNodes)
{
  if (!mRemovedNodes && mTarget) {
    mRemovedNodes = new nsSimpleContentList(mTarget);
  }
  NS_IF_ADDREF(*aRemovedNodes = mRemovedNodes);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  nsCOMPtr<nsIDOMNode> n = do_QueryInterface(mPreviousSibling);
  *aPreviousSibling = n.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetNextSibling(nsIDOMNode** aNextSibling)
{
  nsCOMPtr<nsIDOMNode> n = do_QueryInterface(mNextSibling);
  *aNextSibling = n.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetAttributeName(nsAString& aAttrName)
{
  aAttrName = mAttrName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetAttributeNamespace(nsAString& aAttrNamespace)
{
  aAttrNamespace = mAttrNamespace;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationRecord::GetOldValue(nsAString& aPrevValue)
{
  aPrevValue = mPrevValue;
  return NS_OK;
}

// Observer

NS_IMPL_ADDREF(nsMutationReceiver)
NS_IMPL_RELEASE(nsMutationReceiver)

NS_INTERFACE_MAP_BEGIN(nsMutationReceiver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsMutationReceiver)
NS_INTERFACE_MAP_END

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
  nsIDOMMutationObserver* observer = mObserver;
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
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aModType)
{
  if (nsAutoMutationBatch::IsBatching() ||
      !ObservesAttr(aElement, aNameSpaceID, aAttribute) ||
      aElement->IsInNativeAnonymousSubtree()) {
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
      aContent->IsInNativeAnonymousSubtree()) {
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
                                    PRInt32 aNewIndexInContainer)
{
  nsINode* parent = NODE_FROM(aContainer, aDocument);
  bool wantsChildList = ChildList() && (Subtree() || parent == Target());
  if (!wantsChildList || aFirstNewContent->IsInNativeAnonymousSubtree()) {
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
                                    PRInt32 aIndexInContainer)
{
  nsINode* parent = NODE_FROM(aContainer, aDocument);
  bool wantsChildList = ChildList() && (Subtree() || parent == Target());
  if (!wantsChildList || aChild->IsInNativeAnonymousSubtree()) {
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
                                   PRInt32 aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  if (aChild->IsInNativeAnonymousSubtree()) {
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
        for (PRInt32 i = 0; i < transientReceivers->Count(); ++i) {
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

DOMCI_DATA(MutationObserver, nsDOMMutationObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MutationObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMMutationObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMMutationObserver)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMMutationObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOwner)
  for (PRInt32 i = 0; i < tmp->mReceivers.Count(); ++i) {
    tmp->mReceivers[i]->Disconnect(false);
  }
  tmp->mReceivers.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mPendingMutations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCallback)
  // No need to handle mTransientReceivers
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMMutationObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mReceivers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mPendingMutations)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCallback)
  // No need to handle mTransientReceivers
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsMutationReceiver*
nsDOMMutationObserver::GetReceiverFor(nsINode* aNode, bool aMayCreate)
{
  if (!aMayCreate && !aNode->MayHaveDOMMutationObserver()) {
    return nullptr;
  }

  for (PRInt32 i = 0; i < mReceivers.Count(); ++i) {
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
        if (mReceivers.Count() == PRInt32(aReceivers.Length())) {
          return;
        }
      }                                            
      nsCOMArray<nsMutationReceiver>* transientReceivers = nullptr;
      if (mTransientReceivers.Get(n, &transientReceivers) && transientReceivers) {
        for (PRInt32 i = 0; i < transientReceivers->Count(); ++i) {
          nsMutationReceiver* r = transientReceivers->ObjectAt(i);
          nsMutationReceiver* parent = r->GetParent();
          if (r->Subtree() && parent && !aReceivers.Contains(parent)) {
            aReceivers.AppendElement(parent);
          }
        }
        if (mReceivers.Count() == PRInt32(aReceivers.Length())) {
          return;
        }
      }
    }
    n = n->GetNodeParent();
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
    sScheduledMutationObservers = new nsCOMArray<nsIDOMMutationObserver>;
  }

  bool didInsert = false;
  for (PRInt32 i = 0; i < sScheduledMutationObservers->Count(); ++i) {
    if (static_cast<nsDOMMutationObserver*>((*sScheduledMutationObservers)[i])
          ->mId > mId) {
      sScheduledMutationObservers->InsertObjectAt(this, i);
      didInsert = true;
      break;
    }
  }
  if (!didInsert) {
    sScheduledMutationObservers->AppendObject(this);
  }
}

NS_IMETHODIMP
nsDOMMutationObserver::Observe(nsIDOMNode* aTarget,
                               const JS::Value& aOptions,
                               JSContext* aCx)
{
  nsCOMPtr<nsINode> target = do_QueryInterface(aTarget);
  NS_ENSURE_STATE(target);

  mozilla::dom::MutationObserverInit d;
  nsresult rv = d.Init(aCx, &aOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(d.childList || d.attributes || d.characterData,
                 NS_ERROR_DOM_SYNTAX_ERR);
  NS_ENSURE_TRUE(!d.attributeOldValue || d.attributes,
                 NS_ERROR_DOM_SYNTAX_ERR);
  NS_ENSURE_TRUE(!d.characterDataOldValue || d.characterData,
                 NS_ERROR_DOM_SYNTAX_ERR);

  nsCOMArray<nsIAtom> filters;
  bool allAttrs = true;
  if (!d.attributeFilter.isUndefined()) {
    allAttrs = false;
    rv = nsContentUtils::JSArrayToAtomArray(aCx, d.attributeFilter, filters);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(filters.Count() == 0 || d.attributes,
                   NS_ERROR_DOM_SYNTAX_ERR);
  }

  nsMutationReceiver* r = GetReceiverFor(target, true);
  r->SetChildList(d.childList);
  r->SetAttributes(d.attributes);
  r->SetCharacterData(d.characterData);
  r->SetSubtree(d.subtree);
  r->SetAttributeOldValue(d.attributeOldValue);
  r->SetCharacterDataOldValue(d.characterDataOldValue);
  r->SetAttributeFilter(filters);
  r->SetAllAttributes(allAttrs);
  r->RemoveClones();

#ifdef DEBUG
  for (PRInt32 i = 0; i < mReceivers.Count(); ++i) {
    NS_WARN_IF_FALSE(mReceivers[i]->Target(),
                     "All the receivers should have a target!");
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationObserver::Disconnect()
{
  for (PRInt32 i = 0; i < mReceivers.Count(); ++i) {
    mReceivers[i]->Disconnect(false);
  }
  mReceivers.Clear();
  mCurrentMutations.Clear();
  mPendingMutations.Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationObserver::TakeRecords(nsIVariant** aRetVal)
{
  *aRetVal = TakeRecords().get();
  return NS_OK;
}

already_AddRefed<nsIVariant>
nsDOMMutationObserver::TakeRecords()
{
  nsCOMPtr<nsIWritableVariant> mutations =
    do_CreateInstance("@mozilla.org/variant;1");
  PRInt32 len = mPendingMutations.Count();
  if (len == 0) {
    mutations->SetAsEmptyArray();
  } else {
    nsTArray<nsIDOMMutationRecord*> mods(len);
    for (PRInt32 i = 0; i < len; ++i) {
      mods.AppendElement(mPendingMutations[i]);
    }

    mutations->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                          &NS_GET_IID(nsIDOMMutationRecord),
                          mods.Length(),
                          const_cast<void*>(
                            static_cast<const void*>(mods.Elements())));
    mPendingMutations.Clear();
  }
  return mutations.forget();
}

NS_IMETHODIMP
nsDOMMutationObserver::Initialize(nsISupports* aOwner, JSContext* cx,
                                  JSObject* obj, PRUint32 argc, jsval* argv)
{
  mOwner = do_QueryInterface(aOwner);
  if (!mOwner) {
    NS_WARNING("Unexpected nsIJSNativeInitializer owner");
    return NS_OK;
  }
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(sgo);
  mScriptContext = sgo->GetContext();
  NS_ENSURE_STATE(mScriptContext);
  
  NS_ENSURE_STATE(argc >= 1);
  NS_ENSURE_STATE(!JSVAL_IS_PRIMITIVE(argv[0]));

  nsCOMPtr<nsISupports> tmp;
  nsContentUtils::XPConnect()->WrapJS(cx, JSVAL_TO_OBJECT(argv[0]),
                                      NS_GET_IID(nsIMutationObserverCallback),
                                      getter_AddRefs(tmp));
  mCallback = do_QueryInterface(tmp);
  NS_ENSURE_STATE(mCallback);
  
  return NS_OK;
}

void
nsDOMMutationObserver::HandleMutation()
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(), "Whaat!");
  NS_ASSERTION(mCurrentMutations.IsEmpty(),
               "Still generating MutationRecords?");

  mWaitingForRun = false;

  for (PRInt32 i = 0; i < mReceivers.Count(); ++i) {
    mReceivers[i]->RemoveClones();
  }
  mTransientReceivers.Clear();

  nsPIDOMWindow* outer = mOwner->GetOuterWindow();
  if (!mPendingMutations.Count() || !outer ||
      outer->GetCurrentInnerWindow() != mOwner) {
    mPendingMutations.Clear();
    return;
  }
  nsCxPusher pusher;
  nsCOMPtr<nsIDOMEventTarget> et = do_QueryInterface(mOwner);
  if (!mCallback || !pusher.Push(et)) {
    mPendingMutations.Clear();
    return;
  }

  nsCOMPtr<nsIVariant> mutations = TakeRecords();
  nsAutoMicroTask mt;
  sCurrentObserver = this; // For 'this' handling.
  mCallback->HandleMutations(mutations, this);
  sCurrentObserver = nullptr;
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

  nsCOMArray<nsIDOMMutationObserver>* suppressedObservers = nullptr;

  while (sScheduledMutationObservers) {
    nsCOMArray<nsIDOMMutationObserver>* observers = sScheduledMutationObservers;
    sScheduledMutationObservers = nullptr;
    for (PRInt32 i = 0; i < observers->Count(); ++i) {
      sCurrentObserver = static_cast<nsDOMMutationObserver*>((*observers)[i]);
      if (!sCurrentObserver->Suppressed()) {
        sCurrentObserver->HandleMutation();
      } else {
        if (!suppressedObservers) {
          suppressedObservers = new nsCOMArray<nsIDOMMutationObserver>;
        }
        if (suppressedObservers->IndexOf(sCurrentObserver) < 0) {
          suppressedObservers->AppendObject(sCurrentObserver);
        }
      }
    }
    delete observers;
  }

  if (suppressedObservers) {
    for (PRInt32 i = 0; i < suppressedObservers->Count(); ++i) {
      static_cast<nsDOMMutationObserver*>(suppressedObservers->ObjectAt(i))->
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

  PRUint32 last = sMutationLevel - 1;
  if (!mCurrentMutations[last]) {
    nsDOMMutationRecord* r = new nsDOMMutationRecord(aType);
    mCurrentMutations[last] = r;
    mPendingMutations.AppendObject(r);
    ScheduleForRun();
  }

  NS_ASSERTION(mCurrentMutations[last]->mType.Equals(aType),
               "Unexpected MutationRecord type!");

  return mCurrentMutations[last];
}

nsDOMMutationObserver::~nsDOMMutationObserver()
{
  for (PRInt32 i = 0; i < mReceivers.Count(); ++i) {
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
    nsCOMArray<nsIDOMMutationObserver>& obs =
      sCurrentlyHandlingObservers->ElementAt(sMutationLevel - 1);
    for (PRInt32 i = 0; i < obs.Count(); ++i) {
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
      new nsAutoTArray<nsCOMArray<nsIDOMMutationObserver>, 4>;
  }

  while (sCurrentlyHandlingObservers->Length() < sMutationLevel) {
    sCurrentlyHandlingObservers->InsertElementAt(
      sCurrentlyHandlingObservers->Length());
  }

  PRUint32 last = sMutationLevel - 1;
  if (sCurrentlyHandlingObservers->ElementAt(last).IndexOf(aObserver) < 0) {
    sCurrentlyHandlingObservers->ElementAt(last).AppendObject(aObserver);
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

  PRUint32 len = mObservers.Length();
  for (PRUint32 i = 0; i < len; ++i) {
    nsDOMMutationObserver* ob = mObservers[i].mObserver;
    bool wantsChildList = mObservers[i].mWantsChildList;

    nsRefPtr<nsSimpleContentList> removedList;
    if (wantsChildList) {
      removedList = new nsSimpleContentList(mBatchTarget);
    }

    nsTArray<nsMutationReceiver*> allObservers;
    ob->GetAllSubtreeObserversFor(mBatchTarget, allObservers);

    PRInt32 j = mFromFirstToLast ? 0 : mRemovedNodes.Length() - 1;
    PRInt32 end = mFromFirstToLast ? mRemovedNodes.Length() : -1;
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
        for (PRUint32 k = 0; k < allObservers.Length(); ++k) {
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
      for (PRUint32 i = 0; i < mAddedNodes.Length(); ++i) {
        addedList->AppendElement(mAddedNodes[i]);
      }
      nsDOMMutationRecord* m =
        new nsDOMMutationRecord(NS_LITERAL_STRING("childList"));
      ob->mPendingMutations.AppendObject(m);
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
