/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Report.h"
#include "mozilla/dom/ReportBody.h"
#include "mozilla/dom/ReportingBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Report, mWindow, mBody)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Report)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Report)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Report)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Report::Report(nsPIDOMWindowInner* aWindow, const nsAString& aType,
               const nsAString& aURL, ReportBody* aBody)
    : mWindow(aWindow), mType(aType), mURL(aURL), mBody(aBody) {
  MOZ_ASSERT(aWindow);
}

Report::~Report() = default;

already_AddRefed<Report> Report::Clone() {
  RefPtr<Report> report = new Report(mWindow, mType, mURL, mBody);
  return report.forget();
}

JSObject* Report::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  return Report_Binding::Wrap(aCx, this, aGivenProto);
}

void Report::GetType(nsAString& aType) const { aType = mType; }

void Report::GetUrl(nsAString& aURL) const { aURL = mURL; }

ReportBody* Report::GetBody() const { return mBody; }

}  // namespace dom
}  // namespace mozilla
