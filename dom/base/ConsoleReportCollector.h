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

class ConsoleReportCollector final : public nsIConsoleReportCollector
{
public:
  ConsoleReportCollector();

  void
  AddConsoleReport(uint32_t aErrorFlags, const nsACString& aCategory,
                   nsContentUtils::PropertiesFile aPropertiesFile,
                   const nsACString& aSourceFileURI,
                   uint32_t aLineNumber, uint32_t aColumnNumber,
                   const nsACString& aMessageName,
                   const nsTArray<nsString>& aStringParams) override;

  void
  FlushConsoleReports(nsIDocument* aDocument) override;

  void
  FlushConsoleReports(nsIConsoleReportCollector* aCollector) override;

private:
  ~ConsoleReportCollector();

  struct PendingReport
  {
    PendingReport(uint32_t aErrorFlags, const nsACString& aCategory,
               nsContentUtils::PropertiesFile aPropertiesFile,
               const nsACString& aSourceFileURI, uint32_t aLineNumber,
               uint32_t aColumnNumber, const nsACString& aMessageName,
               const nsTArray<nsString>& aStringParams)
      : mErrorFlags(aErrorFlags)
      , mCategory(aCategory)
      , mPropertiesFile(aPropertiesFile)
      , mSourceFileURI(aSourceFileURI)
      , mLineNumber(aLineNumber)
      , mColumnNumber(aColumnNumber)
      , mMessageName(aMessageName)
      , mStringParams(aStringParams)
    { }

    const uint32_t mErrorFlags;
    const nsCString mCategory;
    const nsContentUtils::PropertiesFile mPropertiesFile;
    const nsCString mSourceFileURI;
    const uint32_t mLineNumber;
    const uint32_t mColumnNumber;
    const nsCString mMessageName;
    const nsTArray<nsString> mStringParams;
  };

  Mutex mMutex;

  // protected by mMutex
  nsTArray<PendingReport> mPendingReports;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
};

} // namespace mozilla

#endif // mozilla_ConsoleReportCollector_h
