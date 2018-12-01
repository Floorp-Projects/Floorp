/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EndpointForReportChild.h"
#include "mozilla/dom/ReportDeliver.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsGlobalWindowInner.h"

namespace mozilla {
namespace dom {

/* static */ void ReportDeliver::Record(nsPIDOMWindowInner* aWindow,
                                        const nsAString& aType,
                                        const nsAString& aGroupName,
                                        const nsAString& aURL,
                                        ReportBody* aBody) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aBody);

  nsAutoString reportBodyJSON;
  aBody->ToJSON(reportBodyJSON);

  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return;
  }

  mozilla::ipc::PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(principal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mozilla::ipc::PBackgroundChild* actorChild =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();

  PEndpointForReportChild* actor =
      actorChild->SendPEndpointForReportConstructor(nsString(aGroupName),
                                                    principalInfo);
  if (NS_WARN_IF(!actor)) {
    return;
  }

  ReportData data;
  data.mType = aType;
  data.mURL = aURL;
  data.mReportBodyJSON = reportBodyJSON;
  data.mPrincipal = principal;
  data.mFailures = 0;

  static_cast<EndpointForReportChild*>(actor)->Initialize(data);
}

/* static */ void ReportDeliver::Fetch(const ReportData& aReportData) {
  // TODO
}

}  // namespace dom
}  // namespace mozilla
