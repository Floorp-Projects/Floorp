/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETEMF_H
#define MOZILLA_GFX_PRINTTARGETEMF_H

#include "PrintTargetSkPDF.h"
#include "mozilla/ipc/Shmem.h"

/* include windows.h for the HDC definitions that we need. */
#include <windows.h>

namespace mozilla {
namespace widget {
  class PDFiumProcessParent;
}
}

namespace mozilla {
namespace gfx {

/**
 * A new subclass of PrintTarget.
 * 1. It uses PrintTargetSkPDF to generate one PDF file for one page.
 * 2. It then passes the FileDescriptor of that generated PDF file to the
 *    PDFium process for EMF conversion.
 * 3. After getting the converted EMF contents from the PDFium process, it then
 *    draws it onto the printer DC to finish one page printing task.
 */
class PrintTargetEMF final : public mozilla::gfx::PrintTarget
{
public:
  typedef gfx::IntSize IntSize;
  typedef mozilla::widget::PDFiumProcessParent PDFiumProcessParent;

  static already_AddRefed<PrintTargetEMF>
  CreateOrNull(HDC aDC, const IntSize& aSizeInPoints);

  nsresult BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName,
                                 int32_t aStartPage,
                                 int32_t aEndPage) final override;
  nsresult EndPrinting() final override;
  nsresult AbortPrinting() final override;
  nsresult BeginPage() final override;
  nsresult EndPage() final override;

  already_AddRefed<DrawTarget>
  MakeDrawTarget(const IntSize& aSize,
                 DrawEventRecorder* aRecorder = nullptr) final override;

  already_AddRefed<DrawTarget>
  GetReferenceDrawTarget(DrawEventRecorder* aRecorder) final override;

  void ConvertToEMFDone(const nsresult& aResult, mozilla::ipc::Shmem&& aEMF);
  bool IsSyncPagePrinting() const final { return false; }
  void ChannelIsBroken() { mChannelBroken = true; }

private:
  PrintTargetEMF(HDC aDC, const IntSize& aSize);
  ~PrintTargetEMF() override;

  nsString mTitle;
  RefPtr<PrintTargetSkPDF> mTargetForCurrentPage;
  nsCOMPtr<nsIFile>        mPDFFileForOnePage;
  RefPtr<PrintTargetSkPDF> mRefTarget;
  PDFiumProcessParent*     mPDFiumProcess;
  HDC mPrinterDC;
  bool mChannelBroken;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETEMF_H */
