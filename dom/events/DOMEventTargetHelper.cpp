/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Sprintf.h"
#include "mozilla/dom/Event.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Likely.h"
#include "MainThreadUtils.h"

namespace mozilla {

using namespace dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(DOMEventTargetHelper)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoString uri;
    if (tmp->mOwnerWindow && tmp->mOwnerWindow->GetExtantDoc()) {
      Unused << tmp->mOwnerWindow->GetExtantDoc()->GetDocumentURI(uri);
    }

    nsXPCOMCycleCollectionParticipant* participant = nullptr;
    CallQueryInterface(tmp, &participant);

    SprintfLiteral(name, "%s %s", participant->ClassName(),
                   NS_ConvertUTF16toUTF8(uri).get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(DOMEventTargetHelper, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  if (tmp->mListenerManager) {
    tmp->mListenerManager->Disconnect();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
  }
  tmp->MaybeDontKeepAlive();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(DOMEventTargetHelper)
  bool hasLiveWrapper = tmp->HasKnownLiveWrapper();
  if (hasLiveWrapper || tmp->IsCertainlyAliveForCC()) {
    if (tmp->mListenerManager) {
      tmp->mListenerManager->MarkForCC();
    }
    if (!hasLiveWrapper && tmp->PreservingWrapper()) {
      tmp->MarkWrapperLive();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(DOMEventTargetHelper)
  return tmp->HasKnownLiveWrapperAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(DOMEventTargetHelper)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMEventTargetHelper)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY(DOMEventTargetHelper)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(DOMEventTargetHelper,
                                                   LastRelease())

DOMEventTargetHelper::DOMEventTargetHelper()
    : mParentObject(nullptr),
      mOwnerWindow(nullptr),
      mHasOrHasHadOwnerWindow(false),
      mIsKeptAlive(false) {}

DOMEventTargetHelper::DOMEventTargetHelper(nsPIDOMWindowInner* aWindow)
    : mParentObject(nullptr),
      mOwnerWindow(nullptr),
      mHasOrHasHadOwnerWindow(false),
      mIsKeptAlive(false) {
  nsIGlobalObject* global = aWindow ? aWindow->AsGlobal() : nullptr;
  BindToOwner(global);
}

DOMEventTargetHelper::DOMEventTargetHelper(nsIGlobalObject* aGlobalObject)
    : mParentObject(nullptr),
      mOwnerWindow(nullptr),
      mHasOrHasHadOwnerWindow(false),
      mIsKeptAlive(false) {
  BindToOwner(aGlobalObject);
}

DOMEventTargetHelper::DOMEventTargetHelper(DOMEventTargetHelper* aOther)
    : mParentObject(nullptr),
      mOwnerWindow(nullptr),
      mHasOrHasHadOwnerWindow(false),
      mIsKeptAlive(false) {
  if (!aOther) {
    BindToOwner(static_cast<nsIGlobalObject*>(nullptr));
    return;
  }
  BindToOwner(aOther->GetParentObject());
  mHasOrHasHadOwnerWindow = aOther->HasOrHasHadOwner();
}

DOMEventTargetHelper::~DOMEventTargetHelper() {
  if (mParentObject) {
    mParentObject->RemoveEventTargetObject(this);
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
  ReleaseWrapper(this);
}

void DOMEventTargetHelper::DisconnectFromOwner() {
  if (mParentObject) {
    mParentObject->RemoveEventTargetObject(this);
  }
  mOwnerWindow = nullptr;
  mParentObject = nullptr;
  // Event listeners can't be handled anymore, so we can release them here.
  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }

  MaybeDontKeepAlive();
}

nsPIDOMWindowInner* DOMEventTargetHelper::GetWindowIfCurrent() const {
  if (NS_FAILED(CheckCurrentGlobalCorrectness())) {
    return nullptr;
  }

  return GetOwner();
}

Document* DOMEventTargetHelper::GetDocumentIfCurrent() const {
  nsPIDOMWindowInner* win = GetWindowIfCurrent();
  if (!win) {
    return nullptr;
  }

  return win->GetDoc();
}

bool DOMEventTargetHelper::ComputeDefaultWantsUntrusted(ErrorResult& aRv) {
  bool wantsUntrusted;
  nsresult rv = WantsUntrusted(&wantsUntrusted);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }
  return wantsUntrusted;
}

bool DOMEventTargetHelper::DispatchEvent(Event& aEvent, CallerType aCallerType,
                                         ErrorResult& aRv) {
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(this, nullptr, &aEvent,
                                                  nullptr, &status);
  bool retval = !aEvent.DefaultPrevented(aCallerType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return retval;
}

nsresult DOMEventTargetHelper::DispatchTrustedEvent(
    const nsAString& aEventName) {
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aEventName, false, false);

  return DispatchTrustedEvent(event);
}

nsresult DOMEventTargetHelper::DispatchTrustedEvent(Event* event) {
  event->SetTrusted(true);

  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

void DOMEventTargetHelper::GetEventTargetParent(
    EventChainPreVisitor& aVisitor) {
  aVisitor.mCanHandle = true;
  aVisitor.SetParentTarget(nullptr, false);
}

nsresult DOMEventTargetHelper::PostHandleEvent(
    EventChainPostVisitor& aVisitor) {
  return NS_OK;
}

EventListenerManager* DOMEventTargetHelper::GetOrCreateListenerManager() {
  if (!mListenerManager) {
    mListenerManager = new EventListenerManager(this);
  }

  return mListenerManager;
}

EventListenerManager* DOMEventTargetHelper::GetExistingListenerManager() const {
  return mListenerManager;
}

nsresult DOMEventTargetHelper::WantsUntrusted(bool* aRetVal) {
  nsresult rv = CheckCurrentGlobalCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Document> doc = GetDocumentIfCurrent();
  // We can let listeners on workers to always handle all the events.
  *aRetVal = (doc && !nsContentUtils::IsChromeDoc(doc)) || !NS_IsMainThread();
  return rv;
}

void DOMEventTargetHelper::EventListenerAdded(nsAtom* aType) {
  MaybeUpdateKeepAlive();
}

void DOMEventTargetHelper::EventListenerRemoved(nsAtom* aType) {
  MaybeUpdateKeepAlive();
}

void DOMEventTargetHelper::KeepAliveIfHasListenersFor(nsAtom* aType) {
  mKeepingAliveTypes.AppendElement(aType);
  MaybeUpdateKeepAlive();
}

void DOMEventTargetHelper::IgnoreKeepAliveIfHasListenersFor(nsAtom* aType) {
  mKeepingAliveTypes.RemoveElement(aType);
  MaybeUpdateKeepAlive();
}

void DOMEventTargetHelper::MaybeUpdateKeepAlive() {
  bool shouldBeKeptAlive = false;

  if (NS_SUCCEEDED(CheckCurrentGlobalCorrectness())) {
    if (!mKeepingAliveTypes.IsEmpty()) {
      for (uint32_t i = 0; i < mKeepingAliveTypes.Length(); ++i) {
        if (HasListenersFor(mKeepingAliveTypes[i])) {
          shouldBeKeptAlive = true;
          break;
        }
      }
    }
  }

  if (shouldBeKeptAlive == mIsKeptAlive) {
    return;
  }

  mIsKeptAlive = shouldBeKeptAlive;
  if (mIsKeptAlive) {
    AddRef();
  } else {
    Release();
  }
}

void DOMEventTargetHelper::MaybeDontKeepAlive() {
  if (mIsKeptAlive) {
    mIsKeptAlive = false;
    Release();
  }
}

void DOMEventTargetHelper::BindToOwner(nsIGlobalObject* aOwner) {
  MOZ_ASSERT(!mParentObject);

  if (aOwner) {
    mParentObject = aOwner;
    aOwner->AddEventTargetObject(this);
    // Let's cache the result of this QI for fast access and off main thread
    // usage
    mOwnerWindow =
        nsCOMPtr<nsPIDOMWindowInner>(do_QueryInterface(aOwner)).get();
    if (mOwnerWindow) {
      mHasOrHasHadOwnerWindow = true;
    }
  }
}

nsresult DOMEventTargetHelper::CheckCurrentGlobalCorrectness() const {
  NS_ENSURE_STATE(!mHasOrHasHadOwnerWindow || mOwnerWindow);

  // Main-thread.
  if (mOwnerWindow && !mOwnerWindow->IsCurrentInnerWindow()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_IsMainThread()) {
    return NS_OK;
  }

  if (!mParentObject) {
    return NS_ERROR_FAILURE;
  }

  if (mParentObject->IsDying()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool DOMEventTargetHelper::HasListenersFor(const nsAString& aType) const {
  return mListenerManager && mListenerManager->HasListenersFor(aType);
}

bool DOMEventTargetHelper::HasListenersFor(nsAtom* aTypeWithOn) const {
  return mListenerManager && mListenerManager->HasListenersFor(aTypeWithOn);
}

}  // namespace mozilla
