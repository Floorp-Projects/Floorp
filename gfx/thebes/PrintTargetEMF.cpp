/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetEMF.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsIFile.h"
#include "mozilla/widget/PDFiumProcessParent.h"
#include "mozilla/widget/PDFiumParent.h"
#include "mozilla/widget/WindowsEMF.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "private/pprio.h"

using mozilla::gfx::DrawTarget;
using mozilla::ipc::FileDescriptor;

namespace mozilla {
namespace gfx {

PrintTargetEMF::PrintTargetEMF(HDC aDC, const IntSize& aSize)
  : PrintTarget(/* not using cairo_surface_t */ nullptr, aSize)
  , mPDFiumProcess(nullptr)
  , mPrinterDC(aDC)
  , mWaitingForEMFConversion(false)
  , mChannelBroken(false)
{
}

PrintTargetEMF::~PrintTargetEMF()
{
  if (mPDFiumProcess) {
    mPDFiumProcess->Delete(mWaitingForEMFConversion);
  }
}

/* static */ already_AddRefed<PrintTargetEMF>
PrintTargetEMF::CreateOrNull(HDC aDC, const IntSize& aSizeInPoints)
{
  return do_AddRef(new PrintTargetEMF(aDC, aSizeInPoints));
}

nsresult
PrintTargetEMF::BeginPrinting(const nsAString& aTitle,
                              const nsAString& aPrintToFileName,
                              int32_t aStartPage,
                              int32_t aEndPage)
{
  mTitle = aTitle;

  const uint32_t DOC_TITLE_LENGTH = MAX_PATH - 1;

  DOCINFOW docinfo;

  nsString titleStr(aTitle);
  if (titleStr.Length() > DOC_TITLE_LENGTH) {
    titleStr.SetLength(DOC_TITLE_LENGTH - 3);
    titleStr.AppendLiteral("...");
  }

  nsString docName(aPrintToFileName);
  docinfo.cbSize = sizeof(docinfo);
  docinfo.lpszDocName = titleStr.Length() > 0 ? titleStr.get() : L"Mozilla Document";
  docinfo.lpszOutput = docName.Length() > 0 ? docName.get() : nullptr;
  docinfo.lpszDatatype = nullptr;
  docinfo.fwType = 0;

  ::StartDocW(mPrinterDC, &docinfo);

  mPDFiumProcess = new PDFiumProcessParent();
  NS_ENSURE_TRUE(mPDFiumProcess->Launch(this), NS_ERROR_FAILURE);

  mChannelBroken = false;

  return NS_OK;
}

nsresult
PrintTargetEMF::EndPrinting()
{
  mPDFiumProcess->GetActor()->EndConversion();
  return (::EndDoc(mPrinterDC) <= 0) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
PrintTargetEMF::AbortPrinting()
{
  return (::AbortDoc(mPrinterDC) <= 0) ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
PrintTargetEMF::BeginPage()
{
  MOZ_ASSERT(!mPDFFileForOnePage && !mTargetForCurrentPage);
  NS_ENSURE_TRUE(!mChannelBroken, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(::StartPage(mPrinterDC) >0, NS_ERROR_FAILURE);

  // We create a new file for each page so that we can make sure each new
  // mPDFFileForOnePage contains one single page.
  nsresult rv =
   NS_OpenAnonymousTemporaryNsIFile(getter_AddRefs(mPDFFileForOnePage));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsAutoCString filePath;
  mPDFFileForOnePage->GetNativePath(filePath);
  auto  stream = MakeUnique<SkFILEWStream>(filePath.get());

  // Creating a new PrintTargetSkPDF for each page so that we can convert each
  // of them into EMF contents individually by the PDFium processes.
  mTargetForCurrentPage = PrintTargetSkPDF::CreateOrNull(Move(stream), mSize);
  mTargetForCurrentPage->BeginPrinting(mTitle, NS_LITERAL_STRING(""), 0, 0);
  mTargetForCurrentPage->BeginPage();

  return NS_OK;
}

nsresult
PrintTargetEMF::EndPage()
{
  NS_ENSURE_TRUE(!mChannelBroken, NS_ERROR_FAILURE);

  mTargetForCurrentPage->EndPage();
  mTargetForCurrentPage->EndPrinting();
  mTargetForCurrentPage->Finish();
  mTargetForCurrentPage = nullptr;

  PRFileDesc* prfile;
  nsresult rv = mPDFFileForOnePage->OpenNSPRFileDesc(PR_RDONLY, PR_IRWXU,
                                                     &prfile);
  NS_ENSURE_SUCCESS(rv, rv);
  FileDescriptor descriptor(FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(prfile)));
  if (!mPDFiumProcess->GetActor()->SendConvertToEMF(descriptor,
                                        ::GetDeviceCaps(mPrinterDC, HORZRES),
                                        ::GetDeviceCaps(mPrinterDC, VERTRES)))
  {
    return NS_ERROR_FAILURE;
  }

  PR_Close(prfile);
  mWaitingForEMFConversion = true;

  return NS_OK;
}

already_AddRefed<DrawTarget>
PrintTargetEMF::MakeDrawTarget(const IntSize& aSize,
                               DrawEventRecorder* aRecorder)
{
  MOZ_ASSERT(!mChannelBroken);
  return mTargetForCurrentPage->MakeDrawTarget(aSize, aRecorder);
}

already_AddRefed<DrawTarget>
PrintTargetEMF::GetReferenceDrawTarget(DrawEventRecorder* aRecorder)
{
  if (!mRefTarget) {
    auto dummy = MakeUnique<SkNullWStream>();
    mRefTarget = PrintTargetSkPDF::CreateOrNull(Move(dummy), mSize);
  }

  if (!mRefDT) {
    mRefDT = mRefTarget->GetReferenceDrawTarget(aRecorder);
  }

  return mRefDT.forget();
}

void
PrintTargetEMF::ConvertToEMFDone(const nsresult& aResult,
                                 mozilla::ipc::Shmem&& aEMF)
{
  MOZ_ASSERT_IF(NS_FAILED(aResult), aEMF.Size<uint8_t>() == 0);
  MOZ_ASSERT(!mChannelBroken, "It is not possible to get conversion callback "
                              "after the channel was broken.");

  mWaitingForEMFConversion = false;
  if (NS_SUCCEEDED(aResult)) {
    if (::StartPage(mPrinterDC) > 0) {
      mozilla::widget::WindowsEMF emf;
      emf.InitFromFileContents(aEMF.get<BYTE>(), aEMF.Size<BYTE>());
      RECT printRect = {0, 0, ::GetDeviceCaps(mPrinterDC, HORZRES),
                        ::GetDeviceCaps(mPrinterDC, VERTRES)};
      DebugOnly<bool> ret = emf.Playback(mPrinterDC, printRect);
      MOZ_ASSERT(ret);

      ::EndPage(mPrinterDC);
    }

    mPDFiumProcess->GetActor()->DeallocShmem(aEMF);
  }

  mPDFFileForOnePage->Remove(/* aRecursive */ false);
  mPDFFileForOnePage = nullptr;

  if (mPageDoneCallback) {
    mPageDoneCallback(aResult);
  }
}

} // namespace gfx
} // namespace mozilla
