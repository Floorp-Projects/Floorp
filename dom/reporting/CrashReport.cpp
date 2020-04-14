/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CrashReport.h"

#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/ReportingHeader.h"
#include "mozilla/dom/ReportDeliver.h"
#include "mozilla/JSONWriter.h"
#include "nsIPrincipal.h"
#include "nsIURIMutator.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

struct StringWriteFunc : public JSONWriteFunc {
  nsCString& mCString;
  explicit StringWriteFunc(nsCString& aCString) : mCString(aCString) {}
  void Write(const char* aStr) override { mCString.Append(aStr); }
};

/* static */
bool CrashReport::Deliver(nsIPrincipal* aPrincipal, bool aIsOOM) {
  MOZ_ASSERT(aPrincipal);

  nsAutoCString endpoint_url;
  ReportingHeader::GetEndpointForReport(NS_LITERAL_STRING("default"),
                                        aPrincipal, endpoint_url);
  if (endpoint_url.IsEmpty()) {
    return false;
  }

  nsCString safe_origin_spec;
  aPrincipal->GetExposableSpec(safe_origin_spec);

  ReportDeliver::ReportData data;
  data.mType = NS_LITERAL_STRING("crash");
  data.mGroupName = NS_LITERAL_STRING("default");
  data.mURL = NS_ConvertUTF8toUTF16(safe_origin_spec);
  data.mCreationTime = TimeStamp::Now();

  Navigator::GetUserAgent(nullptr, aPrincipal, false, data.mUserAgent);
  data.mPrincipal = aPrincipal;
  data.mFailures = 0;
  data.mEndpointURL = endpoint_url;

  nsCString body;
  JSONWriter writer{MakeUnique<StringWriteFunc>(body)};

  writer.Start();
  if (aIsOOM) {
    writer.StringProperty("reason", "oom");
  }
  writer.End();

  data.mReportBodyJSON = body;

  ReportDeliver::Fetch(data);
  return true;
}

}  // namespace dom
}  // namespace mozilla
