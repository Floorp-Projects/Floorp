/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetSkPDF.h"

#include "mozilla/gfx/2D.h"
#include "nsString.h"
#include <vector>

namespace mozilla {
namespace gfx {

PrintTargetSkPDF::PrintTargetSkPDF(const IntSize& aSize,
                                   UniquePtr<SkWStream> aStream)
  : PrintTarget(/* not using cairo_surface_t */ nullptr, aSize)
  , mOStream(Move(aStream))
  , mPageCanvas(nullptr)
{
}

PrintTargetSkPDF::~PrintTargetSkPDF()
{
  Finish(); // ensure stream is flushed

  // Make sure mPDFDoc and mRefPDFDoc are destroyed before our member streams
  // (which they wrap) are destroyed:
  mPDFDoc = nullptr;
  mRefPDFDoc = nullptr;
}

/* static */ already_AddRefed<PrintTargetSkPDF>
PrintTargetSkPDF::CreateOrNull(UniquePtr<SkWStream> aStream,
                               const IntSize& aSizeInPoints)
{
  return do_AddRef(new PrintTargetSkPDF(aSizeInPoints, Move(aStream)));
}

nsresult
PrintTargetSkPDF::BeginPrinting(const nsAString& aTitle,
                                const nsAString& aPrintToFileName,
                                int32_t aStartPage,
                                int32_t aEndPage)
{
  // We need to create the SkPDFDocument here rather than in CreateOrNull
  // because it's only now that we are given aTitle which we want for the
  // PDF metadata.

  SkDocument::PDFMetadata metadata;
  metadata.fTitle = NS_ConvertUTF16toUTF8(aTitle).get();
  metadata.fCreator = "Firefox";
  SkTime::DateTime now;
  SkTime::GetDateTime(&now);
  metadata.fCreation.fEnabled = true;
  metadata.fCreation.fDateTime = now;
  metadata.fModified.fEnabled = true;
  metadata.fModified.fDateTime = now;

  // SkDocument stores a non-owning raw pointer to aStream
  mPDFDoc = SkDocument::MakePDF(mOStream.get(), SK_ScalarDefaultRasterDPI,
                                metadata, /*jpegEncoder*/ nullptr, true);

  return mPDFDoc ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
PrintTargetSkPDF::BeginPage()
{
  mPageCanvas = mPDFDoc->beginPage(mSize.width, mSize.height);

  return !mPageCanvas ? NS_ERROR_FAILURE : PrintTarget::BeginPage();
}

nsresult
PrintTargetSkPDF::EndPage()
{
  mPageCanvas = nullptr;
  mPageDT = nullptr;
  return PrintTarget::EndPage();
}

nsresult
PrintTargetSkPDF::EndPrinting()
{
  mPDFDoc->close();
  if (mRefPDFDoc) {
    mRefPDFDoc->close();
  }
  mPageCanvas = nullptr;
  mPageDT = nullptr;
  return NS_OK;
}

void
PrintTargetSkPDF::Finish()
{
  if (mIsFinished) {
    return;
  }
  mOStream->flush();
  PrintTarget::Finish();
}

already_AddRefed<DrawTarget>
PrintTargetSkPDF::MakeDrawTarget(const IntSize& aSize,
                                 DrawEventRecorder* aRecorder)
{
  if (aRecorder) {
    return PrintTarget::MakeDrawTarget(aSize, aRecorder);
  }
  //MOZ_ASSERT(aSize == mSize, "Should mPageCanvas size match?");
  if (!mPageCanvas) {
    return nullptr;
  }
  mPageDT = Factory::CreateDrawTargetWithSkCanvas(mPageCanvas);
  if (!mPageDT) {
    mPageCanvas = nullptr;
    return nullptr;
  }
  return do_AddRef(mPageDT);
}

already_AddRefed<DrawTarget>
PrintTargetSkPDF::GetReferenceDrawTarget(DrawEventRecorder* aRecorder)
{
  if (!mRefDT) {
    SkDocument::PDFMetadata metadata;
    // SkDocument stores a non-owning raw pointer to aStream
    mRefPDFDoc = SkDocument::MakePDF(&mRefOStream,
                                     SK_ScalarDefaultRasterDPI,
                                     metadata, nullptr, true);
    if (!mRefPDFDoc) {
      return nullptr;
    }
    mRefCanvas = mRefPDFDoc->beginPage(mSize.width, mSize.height);
    if (!mRefCanvas) {
      return nullptr;
    }
    RefPtr<DrawTarget> dt =
      Factory::CreateDrawTargetWithSkCanvas(mRefCanvas);
    if (!dt) {
      return nullptr;
    }
    mRefDT = dt.forget();
  }

  if (aRecorder) {
    if (!mRecordingRefDT) {
      RefPtr<DrawTarget> dt = CreateRecordingDrawTarget(aRecorder, mRefDT);
      if (!dt || !dt->IsValid()) {
        return nullptr;
      }
      mRecordingRefDT = dt.forget();
#ifdef DEBUG
      mRecorder = aRecorder;
#endif
    }
#ifdef DEBUG
    else {
      MOZ_ASSERT(aRecorder == mRecorder,
                 "Caching mRecordingRefDT assumes the aRecorder is an invariant");
    }
#endif

    return do_AddRef(mRecordingRefDT);
  }

  return do_AddRef(mRefDT);
}

} // namespace gfx
} // namespace mozilla
