/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetCG.h"

#include "cairo.h"
#include "cairo-quartz.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "nsObjCExceptions.h"

namespace mozilla {
namespace gfx {

PrintTargetCG::PrintTargetCG(PMPrintSession aPrintSession,
                             const IntSize& aSize)
  : PrintTarget(/* aCairoSurface */ nullptr, aSize)
  , mPrintSession(aPrintSession)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  ::PMRetain(mPrintSession);

  // TODO: Add memory reporting like gfxQuartzSurface.
  //RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

PrintTargetCG::~PrintTargetCG()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPrintSession)
    ::PMRelease(mPrintSession);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

/* static */ already_AddRefed<PrintTargetCG>
PrintTargetCG::CreateOrNull(PMPrintSession aPrintSession, const IntSize& aSize)
{
  if (!Factory::CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<PrintTargetCG> target = new PrintTargetCG(aPrintSession, aSize);

  return target.forget();
}

static size_t
PutBytesNull(void* info, const void* buffer, size_t count)
{
  return count;
}

already_AddRefed<DrawTarget>
PrintTargetCG::GetReferenceDrawTarget(DrawEventRecorder* aRecorder)
{
  if (!mRefDT) {
    const IntSize size(1, 1);

    CGDataConsumerCallbacks callbacks = {PutBytesNull, nullptr};
    CGDataConsumerRef consumer = CGDataConsumerCreate(nullptr, &callbacks);
    CGContextRef pdfContext = CGPDFContextCreate(consumer, nullptr, nullptr);
    CGDataConsumerRelease(consumer);

    cairo_surface_t* similar =
      cairo_quartz_surface_create_for_cg_context(
        pdfContext, size.width, size.height);

    CGContextRelease(pdfContext);

    if (cairo_surface_status(similar)) {
      return nullptr;
    }

    RefPtr<DrawTarget> dt =
      Factory::CreateDrawTargetForCairoSurface(similar, size);

    // The DT addrefs the surface, so we need drop our own reference to it:
    cairo_surface_destroy(similar);

    if (!dt || !dt->IsValid()) {
      return nullptr;
    }

    if (aRecorder) {
      dt = CreateRecordingDrawTarget(aRecorder, dt);
      if (!dt || !dt->IsValid()) {
        return nullptr;
      }
    }

    mRefDT = dt.forget();
  }
  return do_AddRef(mRefDT);
}

nsresult
PrintTargetCG::BeginPage()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  CGContextRef context;
  // This call will fail if we are not called between the PMSessionBeginPage/
  // PMSessionEndPage calls:
  ::PMSessionGetCGGraphicsContext(mPrintSession, &context);

  if (!context) {
    return NS_ERROR_FAILURE;
  }

  unsigned int width = static_cast<unsigned int>(mSize.width);
  unsigned int height = static_cast<unsigned int>(mSize.height);

  // Initially, origin is at bottom-left corner of the paper.
  // Here, we translate it to top-left corner of the paper.
  CGContextTranslateCTM(context, 0, height);
  CGContextScaleCTM(context, 1.0, -1.0);

  cairo_surface_t* surface =
    cairo_quartz_surface_create_for_cg_context(context, width, height);

  if (cairo_surface_status(surface)) {
    return NS_ERROR_FAILURE;
  }

  mCairoSurface = surface;

  return PrintTarget::BeginPage();

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
PrintTargetCG::EndPage()
{
  cairo_surface_finish(mCairoSurface);
  mCairoSurface = nullptr;
  return PrintTarget::EndPage();
}

} // namespace gfx
} // namespace mozilla
