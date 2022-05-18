/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MemoryReportRequest_h_
#define mozilla_dom_MemoryReportRequest_h_

#include "mozilla/dom/MemoryReportTypes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsISupports.h"

#include <functional>

class nsMemoryReporterManager;

namespace mozilla::dom {

class MemoryReport;

class MemoryReportRequestHost final {
 public:
  explicit MemoryReportRequestHost(uint32_t aGeneration);
  ~MemoryReportRequestHost();

  void RecvReport(const MemoryReport& aReport);
  void Finish(uint32_t aGeneration);

 private:
  const uint32_t mGeneration;
  // Non-null if we haven't yet called EndProcessReport() on it.
  RefPtr<nsMemoryReporterManager> mReporterManager;
  bool mSuccess;
};

class MemoryReportRequestClient final : public nsIRunnable {
 public:
  using ReportCallback = std::function<void(const MemoryReport&)>;
  using FinishCallback = std::function<void(const uint32_t&)>;

  NS_DECL_ISUPPORTS

  static void Start(uint32_t aGeneration, bool aAnonymize,
                    bool aMinimizeMemoryUsage,
                    const Maybe<mozilla::ipc::FileDescriptor>& aDMDFile,
                    const nsACString& aProcessString,
                    const ReportCallback& aReportCallback,
                    const FinishCallback& aFinishCallback);

  NS_IMETHOD Run() override;

 private:
  MemoryReportRequestClient(uint32_t aGeneration, bool aAnonymize,
                            const Maybe<mozilla::ipc::FileDescriptor>& aDMDFile,
                            const nsACString& aProcessString,
                            const ReportCallback& aReportCallback,
                            const FinishCallback& aFinishCallback);

 private:
  ~MemoryReportRequestClient();

  uint32_t mGeneration;
  bool mAnonymize;
  mozilla::ipc::FileDescriptor mDMDFile;
  nsCString mProcessString;
  ReportCallback mReportCallback;
  FinishCallback mFinishCallback;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MemoryReportRequest_h_
