/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTarget.h"

#include "cairo.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

PrintTarget::PrintTarget(cairo_surface_t* aCairoSurface, const IntSize& aSize)
  : mCairoSurface(aCairoSurface)
  , mSize(aSize)
  , mIsFinished(false)
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
    dt = CreateRecordingDrawTarget(aRecorder, dt);
    if (!dt || !dt->IsValid()) {
      return nullptr;
    }
  }

  return dt.forget();
}

already_AddRefed<DrawTarget>
PrintTarget::CreateRecordingDrawTarget(DrawEventRecorder* aRecorder,
                                       DrawTarget* aDrawTarget)
{
  MOZ_ASSERT(aRecorder);
  MOZ_ASSERT(aDrawTarget);

  RefPtr<DrawTarget> dt;

  if (aRecorder) {
    // It doesn't really matter what we pass as the DrawTarget here.
    dt = gfx::Factory::CreateRecordingDrawTarget(aRecorder, aDrawTarget);
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

} // namespace gfx
} // namespace mozilla
