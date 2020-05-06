/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ConsoleReportCollector_h
#define mozilla_ConsoleReportCollector_h

#include "mozilla/Mutex.h"
#include "nsIConsoleReportCollector.h"
#include "nsTArray.h"

namespace mozilla {

namespace net {
class ConsoleReportCollected;
}

class ConsoleReportCollector final : public nsIConsoleReportCollector {
 public:
  ConsoleReportCollector();

  void AddConsoleReport(uint32_t aErrorFlags, const nsACString& aCategory,
                        nsContentUtils::PropertiesFile aPropertiesFile,
                        const nsACString& aSourceFileURI, uint32_t aLineNumber,
                        uint32_t aColumnNumber, const nsACString& aMessageName,
                        const nsTArray<nsString>& aStringParams) override;

  void FlushReportsToConsole(
      uint64_t aInnerWindowID,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushReportsToConsoleForServiceWorkerScope(
      const nsACString& aScope,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushConsoleReports(
      dom::Document* aDocument,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushConsoleReports(
      nsILoadGroup* aLoadGroup,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushConsoleReports(nsIConsoleReportCollector* aCollector) override;

  void StealConsoleReports(
      nsTArray<net::ConsoleReportCollected>& aReports) override;

  void ClearConsoleReports() override;

 private:
  ~ConsoleReportCollector();

  struct PendingReport {
    PendingReport(uint32_t aErrorFlags, const nsACString& aCategory,
                  nsContentUtils::PropertiesFile aPropertiesFile,
                  const nsACString& aSourceFileURI, uint32_t aLineNumber,
                  uint32_t aColumnNumber, const nsACString& aMessageName,
                  const nsTArray<nsString>& aStringParams)
        : mErrorFlags(aErrorFlags),
          mCategory(aCategory),
          mPropertiesFile(aPropertiesFile),
          mSourceFileURI(aSourceFileURI),
          mLineNumber(aLineNumber),
          mColumnNumber(aColumnNumber),
          mMessageName(aMessageName),
          mStringParams(aStringParams.Clone()) {}

    const uint32_t mErrorFlags;
    const nsCString mCategory;
    const nsContentUtils::PropertiesFile mPropertiesFile;
    const nsCString mSourceFileURI;
    const uint32_t mLineNumber;
    const uint32_t mColumnNumber;
    const nsCString mMessageName;
    const CopyableTArray<nsString> mStringParams;
  };

  Mutex mMutex;

  // protected by mMutex
  nsTArray<PendingReport> mPendingReports;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
};

}  // namespace mozilla

#endif  // mozilla_ConsoleReportCollector_h
