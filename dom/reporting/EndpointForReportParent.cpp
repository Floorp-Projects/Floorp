/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EndpointForReportParent.h"
#include "mozilla/dom/ReportingHeader.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/Unused.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

EndpointForReportParent::EndpointForReportParent()
    : mPBackgroundThread(NS_GetCurrentThread()), mActive(true) {}

EndpointForReportParent::~EndpointForReportParent() = default;

void EndpointForReportParent::ActorDestroy(ActorDestroyReason aWhy) {
  mActive = false;
}

void EndpointForReportParent::Run(
    const nsAString& aGroupName,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  RefPtr<EndpointForReportParent> self = this;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "EndpointForReportParent::Run",
      [self, aGroupName = nsString(aGroupName), aPrincipalInfo]() {
        nsAutoCString uri;
        ReportingHeader::GetEndpointForReport(aGroupName, aPrincipalInfo, uri);
        self->mPBackgroundThread->Dispatch(NS_NewRunnableFunction(
            "EndpointForReportParent::Answer", [self, uri]() {
              if (self->mActive) {
                Unused << self->Send__delete__(self, uri);
              }
            }));
      }));
}

}  // namespace mozilla::dom
