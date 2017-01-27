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

class nsMemoryReporterManager;

namespace mozilla {
namespace dom {

class MaybeFileDesc;

class MemoryReportRequestHost final
{
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

class MemoryReportRequestClient final : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  static void Start(uint32_t aGeneration,
                    bool aAnonymize,
                    bool aMinimizeMemoryUsage,
                    const MaybeFileDesc& aDMDFile,
                    const nsACString& aProcessString);

  NS_IMETHOD Run() override;

private:
  MemoryReportRequestClient(uint32_t aGeneration,
                            bool aAnonymize,
                            const MaybeFileDesc& aDMDFile,
                            const nsACString& aProcessString);

private:
  ~MemoryReportRequestClient();

  uint32_t mGeneration;
  bool mAnonymize;
  mozilla::ipc::FileDescriptor mDMDFile;
  nsCString mProcessString;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MemoryReportRequest_h_
