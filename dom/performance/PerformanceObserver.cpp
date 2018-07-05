/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceObserver.h"

#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceEntryBinding.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "nsIScriptError.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"
#include "nsString.h"
#include "PerformanceEntry.h"
#include "PerformanceObserverEntryList.h"

using namespace mozilla;
using namespace mozilla::dom;

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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(PerformanceObserver)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceObserver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PerformanceObserver::PerformanceObserver(nsPIDOMWindowInner* aOwner,
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
    nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
    if (!ownerWindow) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    RefPtr<PerformanceObserver> observer =
      new PerformanceObserver(ownerWindow, aCb);
    return observer.forget();
  }

  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);
  MOZ_ASSERT(workerPrivate);

  RefPtr<PerformanceObserver> observer =
    new PerformanceObserver(workerPrivate, aCb);
  return observer.forget();
}

JSObject*
PerformanceObserver::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceObserver_Binding::Wrap(aCx, this, aGivenProto);
}

void
PerformanceObserver::Notify()
{
  if (mQueuedEntries.IsEmpty()) {
    return;
  }
  RefPtr<PerformanceObserverEntryList> list =
    new PerformanceObserverEntryList(this, mQueuedEntries);

  mQueuedEntries.Clear();

  ErrorResult rv;
  mCallback->Call(this, *list, *this, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }
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

static const char16_t *const sValidTypeNames[3] = {
  u"mark",
  u"measure",
  u"resource",
};

void
PerformanceObserver::Observe(const PerformanceObserverInit& aOptions)
{
  if (aOptions.mEntryTypes.IsEmpty()) {
    return;
  }

  nsTArray<nsString> validEntryTypes;

  for (const char16_t* name : sValidTypeNames) {
    nsDependentString validTypeName(name);
    if (aOptions.mEntryTypes.Contains<nsString>(validTypeName) &&
        !validEntryTypes.Contains<nsString>(validTypeName)) {
      validEntryTypes.AppendElement(validTypeName);
    }
  }

  nsAutoString invalidTypesJoined;
  bool addComma = false;
  for (const auto& type : aOptions.mEntryTypes) {
    if (!validEntryTypes.Contains<nsString>(type)) {
      if (addComma) {
        invalidTypesJoined.AppendLiteral(", ");
      }
      addComma = true;
      invalidTypesJoined.Append(type);
    }
  }

  if (!invalidTypesJoined.IsEmpty()) {
    if (!NS_IsMainThread()) {
      nsTArray<nsString> params;
      params.AppendElement(invalidTypesJoined);
      WorkerPrivate::ReportErrorToConsole("UnsupportedEntryTypesIgnored",
                                          params);
    } else {
      nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
        do_QueryInterface(mOwner);
      nsIDocument* document = ownerWindow->GetExtantDoc();
      const char16_t* params[] = { invalidTypesJoined.get() };
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("DOM"), document,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "UnsupportedEntryTypesIgnored", params, 1);
    }
  }

  if (validEntryTypes.IsEmpty()) {
    return;
  }

  mEntryTypes.SwapElements(validEntryTypes);

  mPerformance->AddObserver(this);

  if (aOptions.mBuffered) {
    for (auto entryType : mEntryTypes) {
      nsTArray<RefPtr<PerformanceEntry>> existingEntries;
      mPerformance->GetEntriesByType(entryType, existingEntries);
      if (!existingEntries.IsEmpty()) {
        mQueuedEntries.AppendElements(existingEntries);
      }
    }
  }

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

void
PerformanceObserver::TakeRecords(nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  MOZ_ASSERT(aRetval.IsEmpty());
  aRetval.SwapElements(mQueuedEntries);
}
