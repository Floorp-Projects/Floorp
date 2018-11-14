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
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ReportingObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(ReportingObserver)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ReportingObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReportingObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReportingObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<ReportingObserver>
ReportingObserver::Constructor(const GlobalObject& aGlobal,
                               ReportingObserverCallback& aCallback,
                               const ReportingObserverOptions& aOptions,
                               ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);

  nsTArray<nsString> types;
  if (aOptions.mTypes.WasPassed()) {
    types = aOptions.mTypes.Value();
  }

  RefPtr<ReportingObserver> ro =
    new ReportingObserver(window, aCallback, types, aOptions.mBuffered);
  return ro.forget();
}

ReportingObserver::ReportingObserver(nsPIDOMWindowInner* aWindow,
                                     ReportingObserverCallback& aCallback,
                                     const nsTArray<nsString>& aTypes,
                                     bool aBuffered)
  : mWindow(aWindow)
  , mCallback(&aCallback)
  , mTypes(aTypes)
  , mBuffered(aBuffered)
{
  MOZ_ASSERT(aWindow);
}

ReportingObserver::~ReportingObserver()
{
  Disconnect();
}

JSObject*
ReportingObserver::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ReportingObserver_Binding::Wrap(aCx, this, aGivenProto);
}

void
ReportingObserver::Observe()
{
  // TODO
}

void
ReportingObserver::Disconnect()
{
  // TODO
}

void
ReportingObserver::TakeRecords(nsTArray<RefPtr<Report>>& aRecords)
{
  // TODO
}

} // dom namespace
} // mozilla namespace
