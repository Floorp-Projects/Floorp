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
#include "mozilla/StaticPrefs_dom.h"
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mQueuedEntries)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PerformanceObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mQueuedEntries)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(PerformanceObserver)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceObserver)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

const char UnsupportedEntryTypesIgnoredMsgId[] = "UnsupportedEntryTypesIgnored";
const char AllEntryTypesIgnoredMsgId[] = "AllEntryTypesIgnored";

PerformanceObserver::PerformanceObserver(nsPIDOMWindowInner* aOwner,
                                         PerformanceObserverCallback& aCb)
    : mOwner(aOwner),
      mCallback(&aCb),
      mObserverType(ObserverTypeUndefined),
      mConnected(false) {
  MOZ_ASSERT(mOwner);
  mPerformance = aOwner->GetPerformance();
}

PerformanceObserver::PerformanceObserver(WorkerPrivate* aWorkerPrivate,
                                         PerformanceObserverCallback& aCb)
    : mCallback(&aCb), mObserverType(ObserverTypeUndefined), mConnected(false) {
  MOZ_ASSERT(aWorkerPrivate);
  mPerformance = aWorkerPrivate->GlobalScope()->GetPerformance();
}

PerformanceObserver::~PerformanceObserver() {
  Disconnect();
  MOZ_ASSERT(!mConnected);
}

// static
already_AddRefed<PerformanceObserver> PerformanceObserver::Constructor(
    const GlobalObject& aGlobal, PerformanceObserverCallback& aCb,
    ErrorResult& aRv) {
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

JSObject* PerformanceObserver::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return PerformanceObserver_Binding::Wrap(aCx, this, aGivenProto);
}

void PerformanceObserver::Notify() {
  if (mQueuedEntries.IsEmpty()) {
    return;
  }
  RefPtr<PerformanceObserverEntryList> list =
      new PerformanceObserverEntryList(this, mQueuedEntries);

  mQueuedEntries.Clear();

  ErrorResult rv;
  RefPtr<PerformanceObserverCallback> callback(mCallback);
  callback->Call(this, *list, *this, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
  }
}

void PerformanceObserver::QueueEntry(PerformanceEntry* aEntry) {
  MOZ_ASSERT(aEntry);
  MOZ_ASSERT(ObservesTypeOfEntry(aEntry));

  mQueuedEntries.AppendElement(aEntry);
}

static constexpr nsLiteralString kValidEventTimingNames[2] = {
    u"event"_ns, u"first-input"_ns};

/*
 * Keep this list in alphabetical order.
 * https://w3c.github.io/performance-timeline/#supportedentrytypes-attribute
 */
static constexpr nsLiteralString kValidTypeNames[5] = {
    u"mark"_ns, u"measure"_ns, u"navigation"_ns, u"paint"_ns, u"resource"_ns,
};

void PerformanceObserver::ReportUnsupportedTypesErrorToConsole(
    bool aIsMainThread, const char* msgId, const nsString& aInvalidTypes) {
  if (!aIsMainThread) {
    nsTArray<nsString> params;
    params.AppendElement(aInvalidTypes);
    WorkerPrivate::ReportErrorToConsole(msgId, params);
  } else {
    nsCOMPtr<nsPIDOMWindowInner> ownerWindow = do_QueryInterface(mOwner);
    Document* document = ownerWindow->GetExtantDoc();
    AutoTArray<nsString, 1> params = {aInvalidTypes};
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                    document, nsContentUtils::eDOM_PROPERTIES,
                                    msgId, params);
  }
}

void PerformanceObserver::Observe(const PerformanceObserverInit& aOptions,
                                  ErrorResult& aRv) {
  const Optional<Sequence<nsString>>& maybeEntryTypes = aOptions.mEntryTypes;
  const Optional<nsString>& maybeType = aOptions.mType;
  const Optional<bool>& maybeBuffered = aOptions.mBuffered;

  if (!mPerformance) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!maybeEntryTypes.WasPassed() && !maybeType.WasPassed()) {
    /* Per spec (3.3.1.2), this should be a syntax error. */
    aRv.ThrowTypeError("Can't call observe without `type` or `entryTypes`");
    return;
  }

  if (maybeEntryTypes.WasPassed() &&
      (maybeType.WasPassed() || maybeBuffered.WasPassed())) {
    /* Per spec (3.3.1.3), this, too, should be a syntax error. */
    aRv.ThrowTypeError("Can't call observe with both `type` and `entryTypes`");
    return;
  }

  /* 3.3.1.4.1 */
  if (mObserverType == ObserverTypeUndefined) {
    if (maybeEntryTypes.WasPassed()) {
      mObserverType = ObserverTypeMultiple;
    } else {
      mObserverType = ObserverTypeSingle;
    }
  }

  /* 3.3.1.4.2 */
  if (mObserverType == ObserverTypeSingle && maybeEntryTypes.WasPassed()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_MODIFICATION_ERR);
    return;
  }
  /* 3.3.1.4.3 */
  if (mObserverType == ObserverTypeMultiple && maybeType.WasPassed()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_MODIFICATION_ERR);
    return;
  }

  bool needQueueNotificationObserverTask = false;
  /* 3.3.1.5 */
  if (mObserverType == ObserverTypeMultiple) {
    const Sequence<nsString>& entryTypes = maybeEntryTypes.Value();

    if (entryTypes.IsEmpty()) {
      return;
    }

    /* 3.3.1.5.2 */
    nsTArray<nsString> validEntryTypes;

    if (StaticPrefs::dom_enable_event_timing()) {
      for (const nsLiteralString& name : kValidEventTimingNames) {
        if (entryTypes.Contains(name) && !validEntryTypes.Contains(name)) {
          validEntryTypes.AppendElement(name);
        }
      }
    }
    for (const nsLiteralString& name : kValidTypeNames) {
      if (entryTypes.Contains(name) && !validEntryTypes.Contains(name)) {
        validEntryTypes.AppendElement(name);
      }
    }

    nsAutoString invalidTypesJoined;
    bool addComma = false;
    for (const auto& type : entryTypes) {
      if (!validEntryTypes.Contains<nsString>(type)) {
        if (addComma) {
          invalidTypesJoined.AppendLiteral(", ");
        }
        addComma = true;
        invalidTypesJoined.Append(type);
      }
    }

    if (!invalidTypesJoined.IsEmpty()) {
      ReportUnsupportedTypesErrorToConsole(NS_IsMainThread(),
                                           UnsupportedEntryTypesIgnoredMsgId,
                                           invalidTypesJoined);
    }

    /* 3.3.1.5.3 */
    if (validEntryTypes.IsEmpty()) {
      nsString errorString;
      ReportUnsupportedTypesErrorToConsole(
          NS_IsMainThread(), AllEntryTypesIgnoredMsgId, errorString);
      return;
    }

    /*
     * Registered or not, we clear out the list of options, and start fresh
     * with the one that we are using here. (3.3.1.5.4,5)
     */
    mOptions.Clear();
    mOptions.AppendElement(aOptions);

  } else {
    MOZ_ASSERT(mObserverType == ObserverTypeSingle);
    bool typeValid = false;
    nsString type = maybeType.Value();

    /* 3.3.1.6.2 */
    if (StaticPrefs::dom_enable_event_timing()) {
      for (const nsLiteralString& name : kValidEventTimingNames) {
        if (type == name) {
          typeValid = true;
          break;
        }
      }
    }
    for (const nsLiteralString& name : kValidTypeNames) {
      if (type == name) {
        typeValid = true;
        break;
      }
    }

    if (!typeValid) {
      ReportUnsupportedTypesErrorToConsole(
          NS_IsMainThread(), UnsupportedEntryTypesIgnoredMsgId, type);
      return;
    }

    /* 3.3.1.6.4, 3.3.1.6.4 */
    bool didUpdateOptionsList = false;
    nsTArray<PerformanceObserverInit> updatedOptionsList;
    for (auto& option : mOptions) {
      if (option.mType.WasPassed() && option.mType.Value() == type) {
        updatedOptionsList.AppendElement(aOptions);
        didUpdateOptionsList = true;
      } else {
        updatedOptionsList.AppendElement(option);
      }
    }
    if (!didUpdateOptionsList) {
      updatedOptionsList.AppendElement(aOptions);
    }
    mOptions = std::move(updatedOptionsList);

    /* 3.3.1.6.5 */
    if (maybeBuffered.WasPassed() && maybeBuffered.Value()) {
      nsTArray<RefPtr<PerformanceEntry>> existingEntries;
      mPerformance->GetEntriesByTypeForObserver(type, existingEntries);
      if (!existingEntries.IsEmpty()) {
        mQueuedEntries.AppendElements(existingEntries);
        needQueueNotificationObserverTask = true;
      }
    }
  }
  /* Add ourselves to the list of registered performance
   * observers, if necessary. (3.3.1.5.4,5; 3.3.1.6.4)
   */
  mPerformance->AddObserver(this);

  if (needQueueNotificationObserverTask) {
    mPerformance->QueueNotificationObserversTask();
  }
  mConnected = true;
}

void PerformanceObserver::GetSupportedEntryTypes(
    const GlobalObject& aGlobal, JS::MutableHandle<JSObject*> aObject) {
  nsTArray<nsString> validTypes;
  JS::Rooted<JS::Value> val(aGlobal.Context());

  if (StaticPrefs::dom_enable_event_timing()) {
    for (const nsLiteralString& name : kValidEventTimingNames) {
      validTypes.AppendElement(name);
    }
  }
  for (const nsLiteralString& name : kValidTypeNames) {
    validTypes.AppendElement(name);
  }

  if (!ToJSValue(aGlobal.Context(), validTypes, &val)) {
    /*
     * If this conversion fails, we don't set a result.
     * The spec does not allow us to throw an exception.
     */
    return;
  }
  aObject.set(&val.toObject());
}

bool PerformanceObserver::ObservesTypeOfEntry(PerformanceEntry* aEntry) {
  for (auto& option : mOptions) {
    if (aEntry->ShouldAddEntryToObserverBuffer(option)) {
      return true;
    }
  }
  return false;
}

void PerformanceObserver::Disconnect() {
  if (mConnected) {
    MOZ_ASSERT(mPerformance);
    mPerformance->RemoveObserver(this);
    mOptions.Clear();
    mConnected = false;
  }
}

void PerformanceObserver::TakeRecords(
    nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  MOZ_ASSERT(aRetval.IsEmpty());
  aRetval = std::move(mQueuedEntries);
}
