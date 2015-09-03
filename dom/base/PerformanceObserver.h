/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceObserver_h__
#define mozilla_dom_PerformanceObserver_h__

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "mozilla/nsRefPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsPIDOMWindow;
class PerformanceBase;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
class PerformanceEntry;
class PerformanceObserverCallback;
struct PerformanceObserverInit;
namespace workers {
class WorkerPrivate;
} // namespace workers

class PerformanceObserver final : public nsISupports,
                                  public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PerformanceObserver)

  static already_AddRefed<PerformanceObserver>
  Constructor(const GlobalObject& aGlobal,
              PerformanceObserverCallback& aCb,
              ErrorResult& aRv);

  PerformanceObserver(nsPIDOMWindow* aOwner,
                      PerformanceObserverCallback& aCb);

  PerformanceObserver(workers::WorkerPrivate* aWorkerPrivate,
                      PerformanceObserverCallback& aCb);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const { return mOwner; }

  void Observe(const PerformanceObserverInit& aOptions,
               mozilla::ErrorResult& aRv);

  void Disconnect();

  void Notify();
  void QueueEntry(PerformanceEntry* aEntry);

private:
  ~PerformanceObserver();

  nsCOMPtr<nsISupports> mOwner;
  nsRefPtr<PerformanceObserverCallback> mCallback;
  nsRefPtr<PerformanceBase> mPerformance;
  nsTArray<nsString> mEntryTypes;
  bool mConnected;
  nsTArray<nsRefPtr<PerformanceEntry>> mQueuedEntries;
};

} // namespace dom
} // namespace mozilla

#endif
