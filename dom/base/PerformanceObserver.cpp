/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceObserver.h"

#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/dom/workers/bindings/Performance.h"
#include "nsPerformance.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"
#include "nsString.h"
#include "PerformanceEntry.h"
#include "PerformanceObserverEntryList.h"
#include "WorkerPrivate.h"
#include "WorkerScope.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::workers;

NS_IMPL_CYCLE_COLLECTION_CLASS(PerformanceObserver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PerformanceObserver)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PerformanceObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(PerformanceObserver)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceObserver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PerformanceObserver::PerformanceObserver(nsPIDOMWindow* aOwner,
                                         PerformanceObserverCallback& aCb)
  : mOwner(aOwner)
  , mCallback(&aCb)
  , mConnected(false)
{
  MOZ_ASSERT(mOwner);
  mPerformance = aOwner->GetPerformance();
}

PerformanceObserver::PerformanceObserver(WorkerPrivate* aWorkerPrivate,
                                         PerformanceObserverCallback& aCb)
  : mCallback(&aCb)
  , mConnected(false)
{
  MOZ_ASSERT(aWorkerPrivate);
  mPerformance = aWorkerPrivate->GlobalScope()->GetPerformance();
}

PerformanceObserver::~PerformanceObserver()
{
  Disconnect();
  MOZ_ASSERT(!mConnected);
}

// static
already_AddRefed<PerformanceObserver>
PerformanceObserver::Constructor(const GlobalObject& aGlobal,
                                 PerformanceObserverCallback& aCb,
                                 ErrorResult& aRv)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindow> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
    if (!ownerWindow) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    MOZ_ASSERT(ownerWindow->IsInnerWindow());

    nsRefPtr<PerformanceObserver> observer =
      new PerformanceObserver(ownerWindow, aCb);
    return observer.forget();
  }

  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<PerformanceObserver> observer =
    new PerformanceObserver(workerPrivate, aCb);
  return observer.forget();
}

JSObject*
PerformanceObserver::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceObserverBinding::Wrap(aCx, this, aGivenProto);
}

void
PerformanceObserver::Notify()
{
  if (mQueuedEntries.IsEmpty()) {
    return;
  }
  nsRefPtr<PerformanceObserverEntryList> list =
    new PerformanceObserverEntryList(this, mQueuedEntries);

  ErrorResult rv;
  mCallback->Call(this, *list, *this, rv);
  NS_WARN_IF(rv.Failed());
  mQueuedEntries.Clear();
}

void
PerformanceObserver::QueueEntry(PerformanceEntry* aEntry)
{
  MOZ_ASSERT(aEntry);

  nsAutoString entryType;
  aEntry->GetEntryType(entryType);
  if (!mEntryTypes.Contains<nsString>(entryType)) {
    return;
  }

  mQueuedEntries.AppendElement(aEntry);
}

static nsString sValidTypeNames[7] = {
  NS_LITERAL_STRING("composite"),
  NS_LITERAL_STRING("mark"),
  NS_LITERAL_STRING("measure"),
  NS_LITERAL_STRING("navigation"),
  NS_LITERAL_STRING("render"),
  NS_LITERAL_STRING("resource"),
  NS_LITERAL_STRING("server")
};

void
PerformanceObserver::Observe(const PerformanceObserverInit& aOptions,
                             ErrorResult& aRv)
{
  if (aOptions.mEntryTypes.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }

  nsTArray<nsString> validEntryTypes;

  for (const nsString& validTypeName : sValidTypeNames) {
    if (aOptions.mEntryTypes.Contains<nsString>(validTypeName) &&
        !validEntryTypes.Contains<nsString>(validTypeName)) {
      validEntryTypes.AppendElement(validTypeName);
    }
  }

  if (validEntryTypes.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }

  mEntryTypes = validEntryTypes;

  mPerformance->AddObserver(this);
  mConnected = true;
}

void
PerformanceObserver::Disconnect()
{
  if (mConnected) {
    MOZ_ASSERT(mPerformance);
    mPerformance->RemoveObserver(this);
    mConnected = false;
  }
}
