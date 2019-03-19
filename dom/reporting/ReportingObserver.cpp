/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReportingObserver.h"
#include "mozilla/dom/ReportingBinding.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(ReportingObserver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ReportingObserver)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReports)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ReportingObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReports)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(ReportingObserver)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ReportingObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReportingObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReportingObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<ReportingObserver> ReportingObserver::Constructor(
    const GlobalObject& aGlobal, ReportingObserverCallback& aCallback,
    const ReportingObserverOptions& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);

  nsTArray<nsString> types;
  if (aOptions.mTypes.WasPassed()) {
    types = aOptions.mTypes.Value();
  }

  RefPtr<ReportingObserver> ro =
      new ReportingObserver(window, aCallback, types, aOptions.mBuffered);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  aRv = obs->AddObserver(ro, "memory-pressure", true);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return ro.forget();
}

ReportingObserver::ReportingObserver(nsPIDOMWindowInner* aWindow,
                                     ReportingObserverCallback& aCallback,
                                     const nsTArray<nsString>& aTypes,
                                     bool aBuffered)
    : mWindow(aWindow),
      mCallback(&aCallback),
      mTypes(aTypes),
      mBuffered(aBuffered) {
  MOZ_ASSERT(aWindow);
}

ReportingObserver::~ReportingObserver() { Shutdown(); }

void ReportingObserver::Shutdown() {
  Disconnect();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "memory-pressure");
  }
}

JSObject* ReportingObserver::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return ReportingObserver_Binding::Wrap(aCx, this, aGivenProto);
}

void ReportingObserver::Observe() {
  mWindow->RegisterReportingObserver(this, mBuffered);
}

void ReportingObserver::Disconnect() {
  if (mWindow) {
    mWindow->UnregisterReportingObserver(this);
  }
}

void ReportingObserver::TakeRecords(nsTArray<RefPtr<Report>>& aRecords) {
  mReports.SwapElements(aRecords);
}

void ReportingObserver::MaybeReport(Report* aReport) {
  MOZ_ASSERT(aReport);

  if (!mTypes.IsEmpty()) {
    nsAutoString type;
    aReport->GetType(type);

    if (!mTypes.Contains(type)) {
      return;
    }
  }

  bool wasEmpty = mReports.IsEmpty();

  RefPtr<Report> report = aReport->Clone();
  MOZ_ASSERT(report);

  if (NS_WARN_IF(!mReports.AppendElement(report, fallible))) {
    return;
  }

  if (!wasEmpty) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = mWindow;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "ReportingObserver::MaybeReport",
      // MOZ_CAN_RUN_SCRIPT_BOUNDARY until at least we're able to have
      // Runnable::Run be MOZ_CAN_RUN_SCRIPT.  But even then, having a boundary
      // here might make the most sense.
      [window]()
          MOZ_CAN_RUN_SCRIPT_BOUNDARY { window->NotifyReportingObservers(); });

  NS_DispatchToCurrentThread(r);
}

void ReportingObserver::MaybeNotify() {
  if (mReports.IsEmpty()) {
    return;
  }

  // Let's take the ownership of the reports.
  nsTArray<RefPtr<Report>> list;
  list.SwapElements(mReports);

  Sequence<OwningNonNull<Report>> reports;
  for (Report* report : list) {
    if (NS_WARN_IF(!reports.AppendElement(*report, fallible))) {
      return;
    }
  }

  // We should report if this throws exception. But where?
  RefPtr<ReportingObserverCallback> callback(mCallback);
  callback->Call(reports, *this);
}

NS_IMETHODIMP
ReportingObserver::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, "memory-pressure"));
  mReports.Clear();
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
