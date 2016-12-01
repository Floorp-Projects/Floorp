/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_RemotePrintJobParent_h
#define mozilla_layout_RemotePrintJobParent_h

#include "mozilla/layout/PRemotePrintJobParent.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

class nsDeviceContext;
class nsIPrintSettings;
class nsIWebProgressListener;
class PrintTranslator;

namespace mozilla {
namespace layout {

class RemotePrintJobParent final : public PRemotePrintJobParent
{
public:
  explicit RemotePrintJobParent(nsIPrintSettings* aPrintSettings);

  void ActorDestroy(ActorDestroyReason aWhy) final;

  bool RecvInitializePrint(const nsString& aDocumentTitle,
                           const nsString& aPrintToFile,
                           const int32_t& aStartPage,
                           const int32_t& aEndPage) final;

  bool RecvProcessPage(Shmem&& aStoredPage) final;

  bool RecvFinalizePrint() final;

  bool RecvAbortPrint(const nsresult& aRv) final;

  bool RecvStateChange(const long& aStateFlags,
                       const nsresult& aStatus) final;

  bool RecvProgressChange(const long& aCurSelfProgress,
                          const long& aMaxSelfProgress,
                          const long& aCurTotalProgress,
                          const long& aMaxTotalProgress) final;

  bool RecvStatusChange(const nsresult& aStatus) final;

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

  nsresult InitializePrintDevice(const nsString& aDocumentTitle,
                                 const nsString& aPrintToFile,
                                 const int32_t& aStartPage,
                                 const int32_t& aEndPage);

  nsresult PrintPage(const Shmem& aStoredPage);

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  RefPtr<nsDeviceContext> mPrintDeviceContext;
  UniquePtr<PrintTranslator> mPrintTranslator;
  nsCOMArray<nsIWebProgressListener> mPrintProgressListeners;
};

} // namespace layout
} // namespace mozilla

#endif // mozilla_layout_RemotePrintJobParent_h
