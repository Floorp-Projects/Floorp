/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetCG.h"

#include "cairo.h"
#include "cairo-quartz.h"
#include "mozilla/gfx/HelpersCairo.h"

namespace mozilla {
namespace gfx {

PrintTargetCG::PrintTargetCG(cairo_surface_t* aCairoSurface,
                             const IntSize& aSize)
  : PrintTarget(aCairoSurface, aSize)
{
  // TODO: Add memory reporting like gfxQuartzSurface.
  //RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));
}

/* static */ already_AddRefed<PrintTargetCG>
PrintTargetCG::CreateOrNull(const IntSize& aSize, gfxImageFormat aFormat)
{
  if (!Factory::CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  unsigned int width = static_cast<unsigned int>(aSize.width);
  unsigned int height = static_cast<unsigned int>(aSize.height);

  cairo_format_t cformat = GfxFormatToCairoFormat(aFormat);
  cairo_surface_t* surface =
    cairo_quartz_surface_create(cformat, width, height);

  if (cairo_surface_status(surface)) {
    return nullptr;
  }

  // The new object takes ownership of our surface reference.
  RefPtr<PrintTargetCG> target = new PrintTargetCG(surface, aSize);

  return target.forget();
}

/* static */ already_AddRefed<PrintTargetCG>
PrintTargetCG::CreateOrNull(CGContextRef aContext, const IntSize& aSize)
{
  if (!Factory::CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  unsigned int width = static_cast<unsigned int>(aSize.width);
  unsigned int height = static_cast<unsigned int>(aSize.height);

  cairo_surface_t* surface =
    cairo_quartz_surface_create_for_cg_context(aContext, width, height);

  if (cairo_surface_status(surface)) {
    return nullptr;
  }

  // The new object takes ownership of our surface reference.
  RefPtr<PrintTargetCG> target = new PrintTargetCG(surface, aSize);

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

} // namespace gfx
} // namespace mozilla
