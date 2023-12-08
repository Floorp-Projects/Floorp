/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetCG.h"

#include "cairo.h"
#include "cairo-quartz.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/StaticPrefs_print.h"
#include "nsObjCExceptions.h"
#include "nsString.h"
#include "nsIOutputStream.h"

namespace mozilla::gfx {

static size_t PutBytesNull(void* info, const void* buffer, size_t count) {
  return count;
}

PrintTargetCG::PrintTargetCG(CGContextRef aPrintToStreamContext,
                             PMPrintSession aPrintSession,
                             PMPageFormat aPageFormat,
                             PMPrintSettings aPrintSettings,
                             const IntSize& aSize)
    : PrintTarget(/* aCairoSurface */ nullptr, aSize),
      mPrintToStreamContext(aPrintToStreamContext),
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

  if (mPrintToStreamContext) {
    CGContextRelease(mPrintToStreamContext);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static size_t WriteStreamBytes(void* aInfo, const void* aBuffer,
                               size_t aCount) {
  auto* stream = static_cast<nsIOutputStream*>(aInfo);
  auto* data = static_cast<const char*>(aBuffer);
  size_t remaining = aCount;
  do {
    uint32_t wrote = 0;
    // Handle potential narrowing from size_t to uint32_t.
    uint32_t toWrite = uint32_t(
        std::min(remaining, size_t(std::numeric_limits<uint32_t>::max())));
    if (NS_WARN_IF(NS_FAILED(stream->Write(data, toWrite, &wrote)))) {
      break;
    }
    data += wrote;
    remaining -= size_t(wrote);
  } while (remaining);
  return aCount;
}

static void ReleaseStream(void* aInfo) {
  auto* stream = static_cast<nsIOutputStream*>(aInfo);
  stream->Close();
  NS_RELEASE(stream);
}

static CGContextRef CreatePrintToStreamContext(nsIOutputStream* aOutputStream,
                                               const IntSize& aSize) {
  MOZ_ASSERT(aOutputStream);

  NS_ADDREF(aOutputStream);  // Matched by the NS_RELEASE in ReleaseStream.

  CGRect pageBox{{0.0, 0.0}, {CGFloat(aSize.width), CGFloat(aSize.height)}};
  CGDataConsumerCallbacks callbacks = {WriteStreamBytes, ReleaseStream};
  CGDataConsumerRef consumer = CGDataConsumerCreate(aOutputStream, &callbacks);

  // This metadata is added by the CorePrinting APIs in the non-stream case.
  NSString* bundleName = [NSBundle.mainBundle.localizedInfoDictionary
      objectForKey:(NSString*)kCFBundleNameKey];
  CFMutableDictionaryRef auxiliaryInfo = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  CFDictionaryAddValue(auxiliaryInfo, kCGPDFContextCreator,
                       (__bridge CFStringRef)bundleName);

  CGContextRef pdfContext =
      CGPDFContextCreate(consumer, &pageBox, auxiliaryInfo);
  CGDataConsumerRelease(consumer);
  CFRelease(auxiliaryInfo);
  return pdfContext;
}

/* static */ already_AddRefed<PrintTargetCG> PrintTargetCG::CreateOrNull(
    nsIOutputStream* aOutputStream, PMPrintSession aPrintSession,
    PMPageFormat aPageFormat, PMPrintSettings aPrintSettings,
    const IntSize& aSize) {
  if (!Factory::CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  CGContextRef printToStreamContext = nullptr;
  if (aOutputStream) {
    printToStreamContext = CreatePrintToStreamContext(aOutputStream, aSize);
    if (!printToStreamContext) {
      return nullptr;
    }
  }

  RefPtr<PrintTargetCG> target = new PrintTargetCG(
      printToStreamContext, aPrintSession, aPageFormat, aPrintSettings, aSize);

  return target.forget();
}

already_AddRefed<DrawTarget> PrintTargetCG::GetReferenceDrawTarget() {
  if (!mRefDT) {
    const IntSize size(1, 1);

    CGDataConsumerCallbacks callbacks = {PutBytesNull, nullptr};
    CGDataConsumerRef consumer = CGDataConsumerCreate(nullptr, &callbacks);
    CGContextRef pdfContext = CGPDFContextCreate(consumer, nullptr, nullptr);
    CGDataConsumerRelease(consumer);

    cairo_surface_t* similar = cairo_quartz_surface_create_for_cg_context(
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
    mRefDT = dt.forget();
  }

  return do_AddRef(mRefDT);
}

nsresult PrintTargetCG::BeginPrinting(const nsAString& aTitle,
                                      const nsAString& aPrintToFileName,
                                      int32_t aStartPage, int32_t aEndPage) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (mPrintToStreamContext) {
    return NS_OK;
  }

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

  status = ::PMSessionBeginCGDocumentNoDialog(mPrintSession, mPrintSettings,
                                              mPageFormat);

  return status == noErr ? NS_OK : NS_ERROR_ABORT;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult PrintTargetCG::EndPrinting() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (mPrintToStreamContext) {
    CGContextFlush(mPrintToStreamContext);
    CGPDFContextClose(mPrintToStreamContext);
    return NS_OK;
  }

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

nsresult PrintTargetCG::BeginPage(const IntSize& aSizeInPoints) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  unsigned int width;
  unsigned int height;
  if (StaticPrefs::
          print_save_as_pdf_use_page_rule_size_as_paper_size_enabled()) {
    width = static_cast<unsigned int>(aSizeInPoints.width);
    height = static_cast<unsigned int>(aSizeInPoints.height);
  } else {
    width = static_cast<unsigned int>(mSize.width);
    height = static_cast<unsigned int>(mSize.height);
  }

  CGContextRef context;
  if (mPrintToStreamContext) {
    CGRect bounds = CGRectMake(0, 0, width, height);
    CGContextBeginPage(mPrintToStreamContext, &bounds);
    context = mPrintToStreamContext;
  } else {
    // XXX Why are we calling this if we don't check the return value?
    PMSessionError(mPrintSession);

    // XXX For mixed sheet sizes that aren't simply an orientation switch, we
    // will want to be able to pass a sheet size here, using something like:
    //   PMRect bounds = { 0, 0, double(height), double(width) };
    // But the docs for PMSessionBeginPageNoDialog's `pageFrame` parameter say:
    //   "You should pass NULL, as this parameter is currentlyunsupported."
    // https://developer.apple.com/documentation/applicationservices/1463416-pmsessionbeginpagenodialog?language=objc
    // And indeed, it doesn't appear to do anything.
    // (It seems weird that CGContextBeginPage (above) supports passing a rect,
    // and that that works for setting sheet sizes in PDF output, but the Core
    // Printing API does not.)
    // We can always switch to PrintTargetPDF - we use that for Windows/Linux
    // anyway. But Core Graphics output is better than Cairo's in some cases.
    //
    // For now, we support switching sheet orientation only:
    if (StaticPrefs::
            print_save_as_pdf_use_page_rule_size_as_paper_size_enabled()) {
      ::PMOrientation pageOrientation =
          width < height ? kPMPortrait : kPMLandscape;
      ::PMSetOrientation(mPageFormat, pageOrientation, kPMUnlocked);
      // We don't need to reset the orientation, since we set it for every page.
    }
    OSStatus status =
        ::PMSessionBeginPageNoDialog(mPrintSession, mPageFormat, nullptr);
    if (status != noErr) {
      return NS_ERROR_ABORT;
    }

    // This call will fail if it wasn't called between the PMSessionBeginPage/
    // PMSessionEndPage calls:
    ::PMSessionGetCGGraphicsContext(mPrintSession, &context);

    if (!context) {
      return NS_ERROR_FAILURE;
    }
  }

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

  return PrintTarget::BeginPage(aSizeInPoints);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult PrintTargetCG::EndPage() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  cairo_surface_finish(mCairoSurface);
  mCairoSurface = nullptr;

  if (mPrintToStreamContext) {
    CGContextEndPage(mPrintToStreamContext);
  } else {
    OSStatus status = ::PMSessionEndPageNoDialog(mPrintSession);
    if (status != noErr) {
      return NS_ERROR_ABORT;
    }
  }

  return PrintTarget::EndPage();

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

}  // namespace mozilla::gfx
