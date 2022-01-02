/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetCG.h"

#include "cairo.h"
#include "cairo-quartz.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "nsObjCExceptions.h"
#include "nsString.h"

namespace mozilla {
namespace gfx {

PrintTargetCG::PrintTargetCG(PMPrintSession aPrintSession, PMPageFormat aPageFormat,
                             PMPrintSettings aPrintSettings, const IntSize& aSize)
    : PrintTarget(/* aCairoSurface */ nullptr, aSize),
      mPrintSession(aPrintSession),
      mPageFormat(aPageFormat),
      mPrintSettings(aPrintSettings) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  MOZ_ASSERT(mPrintSession && mPageFormat && mPrintSettings);

  ::PMRetain(mPrintSession);
  ::PMRetain(mPageFormat);
  ::PMRetain(mPrintSettings);

  // TODO: Add memory reporting like gfxQuartzSurface.
  // RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

PrintTargetCG::~PrintTargetCG() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  ::PMRelease(mPrintSession);
  ::PMRelease(mPageFormat);
  ::PMRelease(mPrintSettings);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

/* static */ already_AddRefed<PrintTargetCG> PrintTargetCG::CreateOrNull(
    PMPrintSession aPrintSession, PMPageFormat aPageFormat, PMPrintSettings aPrintSettings,
    const IntSize& aSize) {
  if (!Factory::CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<PrintTargetCG> target =
      new PrintTargetCG(aPrintSession, aPageFormat, aPrintSettings, aSize);

  return target.forget();
}

static size_t PutBytesNull(void* info, const void* buffer, size_t count) { return count; }

already_AddRefed<DrawTarget> PrintTargetCG::GetReferenceDrawTarget() {
  if (!mRefDT) {
    const IntSize size(1, 1);

    CGDataConsumerCallbacks callbacks = {PutBytesNull, nullptr};
    CGDataConsumerRef consumer = CGDataConsumerCreate(nullptr, &callbacks);
    CGContextRef pdfContext = CGPDFContextCreate(consumer, nullptr, nullptr);
    CGDataConsumerRelease(consumer);

    cairo_surface_t* similar =
        cairo_quartz_surface_create_for_cg_context(pdfContext, size.width, size.height);

    CGContextRelease(pdfContext);

    if (cairo_surface_status(similar)) {
      return nullptr;
    }

    RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForCairoSurface(similar, size);

    // The DT addrefs the surface, so we need drop our own reference to it:
    cairo_surface_destroy(similar);

    if (!dt || !dt->IsValid()) {
      return nullptr;
    }
    mRefDT = dt.forget();
  }

  return do_AddRef(mRefDT);
}

nsresult PrintTargetCG::BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName,
                                      int32_t aStartPage, int32_t aEndPage) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Print Core of Application Service sent print job with names exceeding
  // 255 bytes. This is a workaround until fix it.
  // (https://openradar.appspot.com/34428043)
  nsAutoString adjustedTitle;
  PrintTarget::AdjustPrintJobNameForIPP(aTitle, adjustedTitle);

  if (!adjustedTitle.IsEmpty()) {
    CFStringRef cfString = ::CFStringCreateWithCharacters(
        NULL, reinterpret_cast<const UniChar*>(adjustedTitle.BeginReading()),
        adjustedTitle.Length());
    if (cfString) {
      ::PMPrintSettingsSetJobName(mPrintSettings, cfString);
      ::CFRelease(cfString);
    }
  }

  OSStatus status;
  status = ::PMSetFirstPage(mPrintSettings, aStartPage, false);
  NS_ASSERTION(status == noErr, "PMSetFirstPage failed");
  status = ::PMSetLastPage(mPrintSettings, aEndPage, false);
  NS_ASSERTION(status == noErr, "PMSetLastPage failed");

  status = ::PMSessionBeginCGDocumentNoDialog(mPrintSession, mPrintSettings, mPageFormat);

  return status == noErr ? NS_OK : NS_ERROR_ABORT;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult PrintTargetCG::EndPrinting() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  ::PMSessionEndDocumentNoDialog(mPrintSession);
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult PrintTargetCG::AbortPrinting() {
#ifdef DEBUG
  mHasActivePage = false;
#endif
  return EndPrinting();
}

nsresult PrintTargetCG::BeginPage() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  PMSessionError(mPrintSession);
  OSStatus status = ::PMSessionBeginPageNoDialog(mPrintSession, mPageFormat, NULL);
  if (status != noErr) {
    return NS_ERROR_ABORT;
  }

  CGContextRef context;
  // This call will fail if it wasn't called between the PMSessionBeginPage/
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

  cairo_surface_t* surface = cairo_quartz_surface_create_for_cg_context(context, width, height);

  if (cairo_surface_status(surface)) {
    return NS_ERROR_FAILURE;
  }

  mCairoSurface = surface;

  return PrintTarget::BeginPage();

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult PrintTargetCG::EndPage() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  cairo_surface_finish(mCairoSurface);
  mCairoSurface = nullptr;

  OSStatus status = ::PMSessionEndPageNoDialog(mPrintSession);
  if (status != noErr) {
    return NS_ERROR_ABORT;
  }

  return PrintTarget::EndPage();

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

}  // namespace gfx
}  // namespace mozilla
