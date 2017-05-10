/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETSKPDF_H
#define MOZILLA_GFX_PRINTTARGETSKPDF_H

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "PrintTarget.h"
#include "SkCanvas.h"
#include "SkDocument.h"
#include "SkStream.h"

namespace mozilla {
namespace gfx {

/**
 * Skia PDF printing target.
 */
class PrintTargetSkPDF final : public PrintTarget
{
public:
  // The returned PrintTargetSkPDF keeps a raw pointer to the passed SkWStream
  // but does not own it.  Callers are responsible for ensuring that passed
  // stream outlives the returned PrintTarget.
  static already_AddRefed<PrintTargetSkPDF>
  CreateOrNull(UniquePtr<SkWStream> aStream,
               const IntSize& aSizeInPoints);

  virtual nsresult BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName,
                                 int32_t aStartPage,
                                 int32_t aEndPage) override;
  virtual nsresult EndPrinting() override;
  virtual void Finish() override;

  virtual nsresult BeginPage() override;
  virtual nsresult EndPage() override;

  virtual already_AddRefed<DrawTarget>
  MakeDrawTarget(const IntSize& aSize,
                 DrawEventRecorder* aRecorder = nullptr) final;

  virtual already_AddRefed<DrawTarget>
  GetReferenceDrawTarget(DrawEventRecorder* aRecorder) override final;

private:
  PrintTargetSkPDF(const IntSize& aSize,
                   UniquePtr<SkWStream> aStream);
  virtual ~PrintTargetSkPDF();

  // Do not hand out references to this object.  It holds a non-owning
  // reference to mOStreame, so must not outlive mOStream.
  sk_sp<SkDocument> mPDFDoc;

  // The stream that the SkDocument outputs to.
  UniquePtr<SkWStream> mOStream;

  // The current page's SkCanvas and its wrapping DrawTarget:
  // Canvas is owned by mPDFDoc, which handles its deletion.
  SkCanvas* mPageCanvas;
  RefPtr<DrawTarget> mPageDT;

  // Members needed to provide a reference DrawTarget:
  sk_sp<SkDocument> mRefPDFDoc;
  // Canvas owned by mRefPDFDoc, which handles its deletion.
  SkCanvas* mRefCanvas;
  SkDynamicMemoryWStream mRefOStream;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETSKPDF_H */
