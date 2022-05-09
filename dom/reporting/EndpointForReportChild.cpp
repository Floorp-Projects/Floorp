/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EndpointForReportChild.h"

namespace mozilla::dom {

EndpointForReportChild::EndpointForReportChild() = default;

EndpointForReportChild::~EndpointForReportChild() = default;

void EndpointForReportChild::Initialize(
    const ReportDeliver::ReportData& aData) {
  mReportData = aData;
}

mozilla::ipc::IPCResult EndpointForReportChild::Recv__delete__(
    const nsCString& aEndpointURL) {
  if (!aEndpointURL.IsEmpty()) {
    mReportData.mEndpointURL = aEndpointURL;
    ReportDeliver::Fetch(mReportData);
  }

  return IPC_OK();
}

}  // namespace mozilla::dom
