/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MemoryReportRequest_h_
#define mozilla_dom_MemoryReportRequest_h_

#include "mozilla/dom/PMemoryReportRequestChild.h"
#include "mozilla/dom/PMemoryReportRequestParent.h"
#include "nsISupports.h"

class nsMemoryReporterManager;

namespace mozilla {
namespace dom {

class MemoryReportRequestParent : public PMemoryReportRequestParent
{
public:
  explicit MemoryReportRequestParent(uint32_t aGeneration);

  virtual ~MemoryReportRequestParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvReport(const MemoryReport& aReport) override;
  virtual mozilla::ipc::IPCResult Recv__delete__() override;

private:
  const uint32_t mGeneration;
  // Non-null if we haven't yet called EndProcessReport() on it.
  RefPtr<nsMemoryReporterManager> mReporterManager;
};

class MemoryReportRequestChild : public PMemoryReportRequestChild,
                                 public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  MemoryReportRequestChild(bool aAnonymize,
                           const MaybeFileDesc& aDMDFile);
  NS_IMETHOD Run() override;

private:
  ~MemoryReportRequestChild() override;

  bool     mAnonymize;
  FileDescriptor mDMDFile;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MemoryReportRequest_h_
