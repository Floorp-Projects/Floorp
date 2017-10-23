/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_RemotePrintJobChild_h
#define mozilla_layout_RemotePrintJobChild_h

#include "mozilla/layout/PRemotePrintJobChild.h"

#include "mozilla/RefPtr.h"
#include "nsIWebProgressListener.h"

class nsPagePrintTimer;
class nsPrintEngine;

namespace mozilla {
namespace layout {

class RemotePrintJobChild final : public PRemotePrintJobChild
                                , public nsIWebProgressListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  RemotePrintJobChild();

  void ActorDestroy(ActorDestroyReason aWhy) final;

  nsresult InitializePrint(const nsString& aDocumentTitle,
                           const nsString& aPrintToFile,
                           const int32_t& aStartPage,
                           const int32_t& aEndPage);

  mozilla::ipc::IPCResult RecvPrintInitializationResult(
    const nsresult& aRv,
    const FileDescriptor& aFd) final;

  void ProcessPage();

  mozilla::ipc::IPCResult RecvPageProcessed(const FileDescriptor& aFd) final;

  mozilla::ipc::IPCResult RecvAbortPrint(const nsresult& aRv) final;

  void SetPagePrintTimer(nsPagePrintTimer* aPagePrintTimer);

  void SetPrintEngine(nsPrintEngine* aPrintEngine);

  PRFileDesc* GetNextPageFD();

private:
  ~RemotePrintJobChild() final;
  void SetNextPageFD(const mozilla::ipc::FileDescriptor& aFd);

  bool mPrintInitialized = false;
  bool mDestroyed = false;
  nsresult mInitializationResult = NS_OK;
  RefPtr<nsPagePrintTimer> mPagePrintTimer;
  RefPtr<nsPrintEngine> mPrintEngine;
  PRFileDesc* mNextPageFD = nullptr;
};

} // namespace layout
} // namespace mozilla

#endif // mozilla_layout_RemotePrintJobChild_h
