/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReportingUtils.h"
#include "mozilla/dom/ReportBody.h"
#include "mozilla/dom/Report.h"
#include "nsAtom.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

/* static */ void ReportingUtils::Report(nsPIDOMWindowInner* aWindow,
                                         nsAtom* aType, const nsAString& aURL,
                                         ReportBody* aBody) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aBody);

  RefPtr<mozilla::dom::Report> report = new mozilla::dom::Report(
      aWindow, nsDependentAtomString(aType), aURL, aBody);
  aWindow->BroadcastReport(report);
}

}  // namespace dom
}  // namespace mozilla
