/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ConsoleReportCollector.h"

#include "nsIConsoleService.h"
#include "nsIScriptError.h"
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
ConsoleReportCollector::FlushConsoleReports(nsIDocument* aDocument,
                                            ReportAction aAction)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    if (aAction == ReportAction::Forget) {
      mPendingReports.SwapElements(reports);
    } else {
      reports = mPendingReports;
    }
  }

  for (uint32_t i = 0; i < reports.Length(); ++i) {
    PendingReport& report = reports[i];

    // It would be nice if we did not have to do this since ReportToConsole()
    // just turns around and converts it back to a spec.
    nsCOMPtr<nsIURI> uri;
    if (!report.mSourceFileURI.IsEmpty()) {
      nsresult rv = NS_NewURI(getter_AddRefs(uri), report.mSourceFileURI);
      MOZ_ALWAYS_SUCCEEDS(rv);
      if (NS_FAILED(rv)) {
        continue;
      }
    }

    // Convert back from nsTArray<nsString> to the char16_t** format required
    // by our l10n libraries and ReportToConsole. (bug 1219762)
    UniquePtr<const char16_t*[]> params;
    uint32_t paramsLength = report.mStringParams.Length();
    if (paramsLength > 0) {
      params = MakeUnique<const char16_t*[]>(paramsLength);
      for (uint32_t j = 0; j < paramsLength; ++j) {
        params[j] = report.mStringParams[j].get();
      }
    }

    nsContentUtils::ReportToConsole(report.mErrorFlags, report.mCategory,
                                    aDocument, report.mPropertiesFile,
                                    report.mMessageName.get(),
                                    params.get(),
                                    paramsLength, uri, EmptyString(),
                                    report.mLineNumber, report.mColumnNumber);
  }
}

void
ConsoleReportCollector::FlushConsoleReports(nsIConsoleReportCollector* aCollector)
{
  MOZ_ASSERT(aCollector);

  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    mPendingReports.SwapElements(reports);
  }

  for (uint32_t i = 0; i < reports.Length(); ++i) {
    PendingReport& report = reports[i];
    aCollector->AddConsoleReport(report.mErrorFlags, report.mCategory,
                                 report.mPropertiesFile, report.mSourceFileURI,
                                 report.mLineNumber, report.mColumnNumber,
                                 report.mMessageName, report.mStringParams);
  }
}

void
ConsoleReportCollector::FlushReportsByWindowId(uint64_t aWindowId,
                                               ReportAction aAction)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    if (aAction == ReportAction::Forget) {
      mPendingReports.SwapElements(reports);
    } else {
      reports = mPendingReports;
    }
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!consoleService) {
    NS_WARNING("GetConsoleService failed");
    return;
  }

  nsresult rv;
  for (uint32_t i = 0; i < reports.Length(); ++i) {
    PendingReport& report = reports[i];

    nsXPIDLString errorText;
    if (!report.mStringParams.IsEmpty()) {
      rv = nsContentUtils::FormatLocalizedString(report.mPropertiesFile,
                                                 report.mMessageName.get(),
                                                 report.mStringParams,
                                                 errorText);
    } else {
      rv = nsContentUtils::GetLocalizedString(report.mPropertiesFile,
                                              report.mMessageName.get(),
                                              errorText);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    rv = errorObject->InitWithWindowID(errorText,
                                       NS_ConvertUTF8toUTF16(report.mSourceFileURI),
                                       EmptyString(),
                                       report.mLineNumber,
                                       report.mColumnNumber,
                                       report.mErrorFlags,
                                       report.mCategory,
                                       aWindowId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    consoleService->LogMessage(errorObject);
  }
}

void
ConsoleReportCollector::ClearConsoleReports()
{
  MutexAutoLock lock(mMutex);

  mPendingReports.Clear();
}

ConsoleReportCollector::~ConsoleReportCollector()
{
}

} // namespace mozilla
