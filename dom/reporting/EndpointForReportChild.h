/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EndpointForReportChild_h
#define mozilla_dom_EndpointForReportChild_h

#include "mozilla/dom/ReportDeliver.h"
#include "mozilla/dom/PEndpointForReportChild.h"

namespace mozilla::dom {

class EndpointForReport;

class EndpointForReportChild final : public PEndpointForReportChild {
 public:
  EndpointForReportChild();
  ~EndpointForReportChild();

  void Initialize(const ReportDeliver::ReportData& aReportData);

  mozilla::ipc::IPCResult Recv__delete__(const nsCString& aEndpointURL);

 private:
  ReportDeliver::ReportData mReportData;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EndpointForReportChild_h
