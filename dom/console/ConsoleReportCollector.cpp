/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ConsoleReportCollector.h"

#include "mozilla/dom/Document.h"
#include "mozilla/net/NeckoChannelParams.h"

#include "ConsoleUtils.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"

namespace mozilla {

using mozilla::dom::ConsoleUtils;

NS_IMPL_ISUPPORTS(ConsoleReportCollector, nsIConsoleReportCollector)

ConsoleReportCollector::ConsoleReportCollector()
    : mMutex("mozilla::ConsoleReportCollector") {}

void ConsoleReportCollector::AddConsoleReport(
    uint32_t aErrorFlags, const nsACString& aCategory,
    nsContentUtils::PropertiesFile aPropertiesFile,
    const nsACString& aSourceFileURI, uint32_t aLineNumber,
    uint32_t aColumnNumber, const nsACString& aMessageName,
    const nsTArray<nsString>& aStringParams) {
  // any thread
  MutexAutoLock lock(mMutex);

  mPendingReports.EmplaceBack(aErrorFlags, aCategory, aPropertiesFile,
                              aSourceFileURI, aLineNumber, aColumnNumber,
                              aMessageName, aStringParams);
}

void ConsoleReportCollector::FlushReportsToConsole(uint64_t aInnerWindowID,
                                                   ReportAction aAction) {
  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    if (aAction == ReportAction::Forget) {
      reports = std::move(mPendingReports);
    } else {
      reports = mPendingReports.Clone();
    }
  }

  for (uint32_t i = 0; i < reports.Length(); ++i) {
    PendingReport& report = reports[i];

    nsAutoString errorText;
    nsresult rv;
    if (!report.mStringParams.IsEmpty()) {
      rv = nsContentUtils::FormatLocalizedString(
          report.mPropertiesFile, report.mMessageName.get(),
          report.mStringParams, errorText);
    } else {
      rv = nsContentUtils::GetLocalizedString(
          report.mPropertiesFile, report.mMessageName.get(), errorText);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    // It would be nice if we did not have to do this since ReportToConsole()
    // just turns around and converts it back to a spec.
    nsCOMPtr<nsIURI> uri;
    if (!report.mSourceFileURI.IsEmpty()) {
      nsresult rv = NS_NewURI(getter_AddRefs(uri), report.mSourceFileURI);
      if (NS_FAILED(rv)) {
        NS_WARNING(nsPrintfCString("Failed to transform %s to uri",
                                   report.mSourceFileURI.get())
                       .get());
        continue;
      }
    }

    nsContentUtils::ReportToConsoleByWindowID(
        errorText, report.mErrorFlags, report.mCategory, aInnerWindowID, uri,
        u""_ns, report.mLineNumber, report.mColumnNumber);
  }
}

void ConsoleReportCollector::FlushReportsToConsoleForServiceWorkerScope(
    const nsACString& aScope, ReportAction aAction) {
  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    if (aAction == ReportAction::Forget) {
      reports = std::move(mPendingReports);
    } else {
      reports = mPendingReports.Clone();
    }
  }

  for (uint32_t i = 0; i < reports.Length(); ++i) {
    PendingReport& report = reports[i];

    nsAutoString errorText;
    nsresult rv;
    if (!report.mStringParams.IsEmpty()) {
      rv = nsContentUtils::FormatLocalizedString(
          report.mPropertiesFile, report.mMessageName.get(),
          report.mStringParams, errorText);
    } else {
      rv = nsContentUtils::GetLocalizedString(
          report.mPropertiesFile, report.mMessageName.get(), errorText);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    ConsoleUtils::Level level = ConsoleUtils::eLog;
    switch (report.mErrorFlags) {
      case nsIScriptError::errorFlag:
        level = ConsoleUtils::eError;
        break;
      case nsIScriptError::warningFlag:
        level = ConsoleUtils::eWarning;
        break;
      default:
        // default to log otherwise
        break;
    }

    ConsoleUtils::ReportForServiceWorkerScope(
        NS_ConvertUTF8toUTF16(aScope), errorText,
        NS_ConvertUTF8toUTF16(report.mSourceFileURI), report.mLineNumber,
        report.mColumnNumber, level);
  }
}

void ConsoleReportCollector::FlushConsoleReports(dom::Document* aDocument,
                                                 ReportAction aAction) {
  MOZ_ASSERT(NS_IsMainThread());

  FlushReportsToConsole(aDocument ? aDocument->InnerWindowID() : 0, aAction);
}

void ConsoleReportCollector::FlushConsoleReports(nsILoadGroup* aLoadGroup,
                                                 ReportAction aAction) {
  FlushReportsToConsole(nsContentUtils::GetInnerWindowID(aLoadGroup), aAction);
}

void ConsoleReportCollector::FlushConsoleReports(
    nsIConsoleReportCollector* aCollector) {
  MOZ_ASSERT(aCollector);

  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    reports = std::move(mPendingReports);
  }

  for (uint32_t i = 0; i < reports.Length(); ++i) {
    PendingReport& report = reports[i];
    aCollector->AddConsoleReport(
        report.mErrorFlags, report.mCategory, report.mPropertiesFile,
        report.mSourceFileURI, report.mLineNumber, report.mColumnNumber,
        report.mMessageName,
        static_cast<const nsTArray<nsString>&>(report.mStringParams));
  }
}

void ConsoleReportCollector::StealConsoleReports(
    nsTArray<net::ConsoleReportCollected>& aReports) {
  aReports.Clear();

  nsTArray<PendingReport> reports;

  {
    MutexAutoLock lock(mMutex);
    reports = std::move(mPendingReports);
  }

  for (const PendingReport& report : reports) {
    aReports.AppendElement(net::ConsoleReportCollected(
        report.mErrorFlags, report.mCategory, report.mPropertiesFile,
        report.mSourceFileURI, report.mLineNumber, report.mColumnNumber,
        report.mMessageName, report.mStringParams));
  }
}

void ConsoleReportCollector::ClearConsoleReports() {
  MutexAutoLock lock(mMutex);

  mPendingReports.Clear();
}

ConsoleReportCollector::~ConsoleReportCollector() = default;

}  // namespace mozilla
