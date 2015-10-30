/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ConsoleReportCollector.h"

#include "nsNetUtil.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(ConsoleReportCollector, nsIConsoleReportCollector)

ConsoleReportCollector::ConsoleReportCollector()
  : mMutex("mozilla::ConsoleReportCollector")
{
}

void
ConsoleReportCollector::AddConsoleReport(uint32_t aErrorFlags,
                                         const nsACString& aCategory,
                                         nsContentUtils::PropertiesFile aPropertiesFile,
                                         const nsACString& aSourceFileURI,
                                         uint32_t aLineNumber,
                                         uint32_t aColumnNumber,
                                         const nsACString& aMessageName,
                                         const nsTArray<nsString>& aStringParams)
{
  // any thread
  MutexAutoLock lock(mMutex);

  mPendingReports.AppendElement(PendingReport(aErrorFlags, aCategory,
                                              aPropertiesFile, aSourceFileURI,
                                              aLineNumber, aColumnNumber,
                                              aMessageName, aStringParams));
}

void
ConsoleReportCollector::FlushConsoleReports(nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);

  for (uint32_t i = 0; i < mPendingReports.Length(); ++i) {
    PendingReport& report = mPendingReports[i];

    // It would be nice if we did not have to do this since ReportToConsole()
    // just turns around and converts it back to a spec.
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), report.mSourceFileURI);
    if (NS_FAILED(rv)) {
      continue;
    }

    // Convert back from nsTArray<nsString> to the char16_t** format required
    // by our l10n libraries and ReportToConsole. (bug 1219762)
    UniquePtr<char16_t*> params;
    uint32_t paramsLength = report.mStringParams.Length();
    if (paramsLength > 0) {
      params.reset(new char16_t*[paramsLength]);
      for (uint32_t j = 0; j < paramsLength; ++j) {
        params.get()[j] = const_cast<char16_t*>(report.mStringParams[j].get());
      }
    }

    nsContentUtils::ReportToConsole(report.mErrorFlags, report.mCategory,
                                    aDocument, report.mPropertiesFile,
                                    report.mMessageName.get(),
                                    const_cast<const char16_t**>(params.get()),
                                    paramsLength, uri, EmptyString(),
                                    report.mLineNumber, report.mColumnNumber);
  }

  mPendingReports.Clear();
}

ConsoleReportCollector::~ConsoleReportCollector()
{
}

} // namespace mozilla
