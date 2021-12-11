/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceObserver_h__
#define mozilla_dom_PerformanceObserver_h__

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "mozilla/dom/PerformanceObserverBinding.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
class Performance;
class PerformanceEntry;
class PerformanceObserverCallback;
class WorkerPrivate;

class PerformanceObserver final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PerformanceObserver)

  static already_AddRefed<PerformanceObserver> Constructor(
      const GlobalObject& aGlobal, PerformanceObserverCallback& aCb,
      ErrorResult& aRv);

  PerformanceObserver(nsPIDOMWindowInner* aOwner,
                      PerformanceObserverCallback& aCb);

  PerformanceObserver(WorkerPrivate* aWorkerPrivate,
                      PerformanceObserverCallback& aCb);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const { return mOwner; }

  void Observe(const PerformanceObserverInit& aOptions, ErrorResult& aRv);
  static void GetSupportedEntryTypes(const GlobalObject& aGlobal,
                                     JS::MutableHandle<JSObject*> aObject);

  void Disconnect();

  void TakeRecords(nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  MOZ_CAN_RUN_SCRIPT void Notify();
  void QueueEntry(PerformanceEntry* aEntry);

  bool ObservesTypeOfEntry(PerformanceEntry* aEntry);

 private:
  void ReportUnsupportedTypesErrorToConsole(bool aIsMainThread,
                                            const char* msgId,
                                            const nsString& aInvalidTypes);
  ~PerformanceObserver();

  nsCOMPtr<nsISupports> mOwner;
  RefPtr<PerformanceObserverCallback> mCallback;
  RefPtr<Performance> mPerformance;
  nsTArray<nsString> mEntryTypes;
  nsTArray<PerformanceObserverInit> mOptions;
  enum {
    ObserverTypeUndefined,
    ObserverTypeSingle,
    ObserverTypeMultiple,
  } mObserverType;
  /*
   * This is also known as registered, in the spec.
   */
  bool mConnected;
  nsTArray<RefPtr<PerformanceEntry>> mQueuedEntries;
};

}  // namespace dom
}  // namespace mozilla

#endif
