/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_RemotePrintJobParent_h
#define mozilla_layout_RemotePrintJobParent_h

#include "mozilla/layout/PRemotePrintJobParent.h"
#include "mozilla/layout/printing/DrawEventRecorder.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/gfx/CrossProcessPaint.h"

class nsDeviceContext;
class nsIPrintSettings;
class nsIWebProgressListener;

namespace mozilla {
namespace layout {

class PrintTranslator;

class RemotePrintJobParent final : public PRemotePrintJobParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemotePrintJobParent);

  explicit RemotePrintJobParent(nsIPrintSettings* aPrintSettings);

  void ActorDestroy(ActorDestroyReason aWhy) final;

  mozilla::ipc::IPCResult RecvInitializePrint(const nsAString& aDocumentTitle,
                                              const int32_t& aStartPage,
                                              const int32_t& aEndPage) final;

  mozilla::ipc::IPCResult RecvProcessPage(nsTArray<uint64_t>&& aDeps) final;

  mozilla::ipc::IPCResult RecvFinalizePrint() final;

  mozilla::ipc::IPCResult RecvAbortPrint(const nsresult& aRv) final;

  mozilla::ipc::IPCResult RecvStateChange(const long& aStateFlags,
                                          const nsresult& aStatus) final;

  mozilla::ipc::IPCResult RecvProgressChange(
      const long& aCurSelfProgress, const long& aMaxSelfProgress,
      const long& aCurTotalProgress, const long& aMaxTotalProgress) final;

  mozilla::ipc::IPCResult RecvStatusChange(const nsresult& aStatus) final;

  /**
   * Register a progress listener to receive print progress updates.
   *
   * @param aListener the progress listener to register. Must not be null.
   */
  void RegisterListener(nsIWebProgressListener* aListener);

  /**
   * @return the print settings for this remote print job.
   */
  already_AddRefed<nsIPrintSettings> GetPrintSettings();

 private:
  ~RemotePrintJobParent() final;

  nsresult InitializePrintDevice(const nsAString& aDocumentTitle,
                                 const int32_t& aStartPage,
                                 const int32_t& aEndPage);

  nsresult PrepareNextPageFD(FileDescriptor* aFd);

  nsresult PrintPage(
      PRFileDescStream& aRecording,
      gfx::CrossProcessPaint::ResolvedFragmentMap* aFragments = nullptr);
  void FinishProcessingPage(
      gfx::CrossProcessPaint::ResolvedFragmentMap* aFragments = nullptr);

  /**
   * Called to notify our corresponding RemotePrintJobChild once we've
   * finished printing a page.
   */
  void PageDone(nsresult aResult);

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  RefPtr<nsDeviceContext> mPrintDeviceContext;
  UniquePtr<PrintTranslator> mPrintTranslator;
  nsCOMArray<nsIWebProgressListener> mPrintProgressListeners;
  PRFileDescStream mCurrentPageStream;
  bool mIsDoingPrinting;
};

}  // namespace layout
}  // namespace mozilla

#endif  // mozilla_layout_RemotePrintJobParent_h
