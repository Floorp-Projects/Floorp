/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetEMF.h"
#include "nsAnonymousTemporaryFile.h"
#include "nsIFile.h"

using mozilla::gfx::DrawTarget;

namespace mozilla {
namespace gfx {

PrintTargetEMF::PrintTargetEMF(HDC aDC, const IntSize& aSize)
  : PrintTarget(/* not using cairo_surface_t */ nullptr, aSize)
  , mPrinterDC(aDC)
{
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

  return NS_OK;
}

nsresult
PrintTargetEMF::EndPrinting()
{
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
  mTargetForCurrentPage->EndPage();
  mTargetForCurrentPage->EndPrinting();
  mTargetForCurrentPage->Finish();
  mTargetForCurrentPage = nullptr;

  // TODO: pass mPDFFileForOnePage to the PDFium process.

  mPDFFileForOnePage->Remove(/* aRecursive */ false);
  mPDFFileForOnePage = nullptr;

  // TODO: we should call EndPage(mPrinterDC), but not here. We should call it
  // after the PDFium process calls Send ConvertToEMFDone.

  return NS_OK;
}

already_AddRefed<DrawTarget>
PrintTargetEMF::MakeDrawTarget(const IntSize& aSize,
                               DrawEventRecorder* aRecorder)
{
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

} // namespace gfx
} // namespace mozilla
