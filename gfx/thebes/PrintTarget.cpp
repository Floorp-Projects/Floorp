/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTarget.h"

#include "cairo.h"
#ifdef CAIRO_HAS_QUARTZ_SURFACE
#include "cairo-quartz.h"
#endif
#ifdef CAIRO_HAS_WIN32_SURFACE
#include "cairo-win32.h"
#endif
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/gfx/Logging.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUTF8Utils.h"

// IPP spec disallow the job-name which is over 255 characters.
// RFC: https://tools.ietf.org/html/rfc2911#section-4.1.2
#define IPP_JOB_NAME_LIMIT_LENGTH 255

namespace mozilla {
namespace gfx {

PrintTarget::PrintTarget(cairo_surface_t* aCairoSurface, const IntSize& aSize)
  : mCairoSurface(aCairoSurface)
  , mSize(aSize)
  , mIsFinished(false)
#ifdef DEBUG
  , mHasActivePage(false)
#endif

{
#if 0
  // aCairoSurface is null when our PrintTargetThebes subclass's ctor calls us.
  // Once PrintTargetThebes is removed, enable this assertion.
  MOZ_ASSERT(aCairoSurface && !cairo_surface_status(aCairoSurface),
             "CreateOrNull factory methods should not call us without a "
             "valid cairo_surface_t*");
#endif

  // CreateOrNull factory methods hand over ownership of aCairoSurface,
  // so we don't call cairo_surface_reference(aSurface) here.

  // This code was copied from gfxASurface::Init:
#ifdef MOZ_TREE_CAIRO
  if (mCairoSurface &&
      cairo_surface_get_content(mCairoSurface) != CAIRO_CONTENT_COLOR) {
    cairo_surface_set_subpixel_antialiasing(mCairoSurface,
                                            CAIRO_SUBPIXEL_ANTIALIASING_DISABLED);
  }
#endif
}

PrintTarget::~PrintTarget()
{
  // null surfaces are allowed here
  cairo_surface_destroy(mCairoSurface);
  mCairoSurface = nullptr;
}

already_AddRefed<DrawTarget>
PrintTarget::MakeDrawTarget(const IntSize& aSize,
                            DrawEventRecorder* aRecorder)
{
  MOZ_ASSERT(mCairoSurface,
             "We shouldn't have been constructed without a cairo surface");

  // This should not be called outside of BeginPage()/EndPage() calls since
  // some backends can only provide a valid DrawTarget at that time.
  MOZ_ASSERT(mHasActivePage, "We can't guarantee a valid DrawTarget");

  if (cairo_surface_status(mCairoSurface)) {
    return nullptr;
  }

  // Note than aSize may not be the same as mSize (the size of mCairoSurface).
  // See the comments in our header.  If the sizes are different a clip will
  // be applied to mCairoSurface.
  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForCairoSurface(mCairoSurface, aSize);
  if (!dt || !dt->IsValid()) {
    return nullptr;
  }

  if (aRecorder) {
    dt = CreateWrapAndRecordDrawTarget(aRecorder, dt);
    if (!dt || !dt->IsValid()) {
      return nullptr;
    }
  }

  return dt.forget();
}

already_AddRefed<DrawTarget>
PrintTarget::GetReferenceDrawTarget()
{
  if (!mRefDT) {
    const IntSize size(1, 1);

    cairo_surface_t* similar;
    switch (cairo_surface_get_type(mCairoSurface)) {
#ifdef CAIRO_HAS_WIN32_SURFACE
    case CAIRO_SURFACE_TYPE_WIN32:
      similar = cairo_win32_surface_create_with_dib(
        CairoContentToCairoFormat(cairo_surface_get_content(mCairoSurface)),
        size.width, size.height);
      break;
#endif
#ifdef CAIRO_HAS_QUARTZ_SURFACE
    case CAIRO_SURFACE_TYPE_QUARTZ:
      similar = cairo_quartz_surface_create_cg_layer(
                  mCairoSurface, cairo_surface_get_content(mCairoSurface),
                  size.width, size.height);
      break;
#endif
    default:
      similar = cairo_surface_create_similar(
                  mCairoSurface, cairo_surface_get_content(mCairoSurface),
                  size.width, size.height);
      break;
    }

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

/* static */
void
PrintTarget::AdjustPrintJobNameForIPP(const nsAString& aJobName,
                                      nsCString& aAdjustedJobName)
{
  CopyUTF16toUTF8(aJobName, aAdjustedJobName);

  if (aAdjustedJobName.Length() > IPP_JOB_NAME_LIMIT_LENGTH) {
    uint32_t length =
      RewindToPriorUTF8Codepoint(aAdjustedJobName.get(),
                                 (IPP_JOB_NAME_LIMIT_LENGTH - 3U));
    aAdjustedJobName.SetLength(length);
    aAdjustedJobName.AppendLiteral("...");
  }
}

/* static */
void
PrintTarget::AdjustPrintJobNameForIPP(const nsAString& aJobName,
                                      nsString& aAdjustedJobName)
{
  nsAutoCString jobName;
  AdjustPrintJobNameForIPP(aJobName, jobName);

  CopyUTF8toUTF16(jobName, aAdjustedJobName);
}

/* static */ already_AddRefed<DrawTarget>
PrintTarget::CreateWrapAndRecordDrawTarget(DrawEventRecorder* aRecorder,
                                       DrawTarget* aDrawTarget)
{
  MOZ_ASSERT(aRecorder);
  MOZ_ASSERT(aDrawTarget);

  RefPtr<DrawTarget> dt;

  if (aRecorder) {
    // It doesn't really matter what we pass as the DrawTarget here.
    dt = gfx::Factory::CreateWrapAndRecordDrawTarget(aRecorder, aDrawTarget);
  }

  if (!dt || !dt->IsValid()) {
    gfxCriticalNote
      << "Failed to create a recording DrawTarget for PrintTarget";
    return nullptr;
  }

  return dt.forget();
}

void
PrintTarget::Finish()
{
  if (mIsFinished) {
    return;
  }
  mIsFinished = true;

  // null surfaces are allowed here
  cairo_surface_finish(mCairoSurface);
}

void
PrintTarget::RegisterPageDoneCallback(PageDoneCallback&& aCallback)
{
  MOZ_ASSERT(aCallback && !IsSyncPagePrinting());
  mPageDoneCallback = std::move(aCallback);
}

void
PrintTarget::UnregisterPageDoneCallback()
{
  mPageDoneCallback = nullptr;
}

} // namespace gfx
} // namespace mozilla
