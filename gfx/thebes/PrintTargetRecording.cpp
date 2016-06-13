/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetRecording.h"

#include "cairo.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

PrintTargetRecording::PrintTargetRecording(cairo_surface_t* aCairoSurface,
                                           const IntSize& aSize)
  : PrintTarget(aCairoSurface, aSize)
{
}

/* static */ already_AddRefed<PrintTargetRecording>
PrintTargetRecording::CreateOrNull(const IntSize& aSize)
{
  if (!Factory::CheckSurfaceSize(aSize)) {
    return nullptr;
  }

  // Perhaps surprisingly, this surface is never actually drawn to.  This class
  // creates a DrawTargetRecording using CreateRecordingDrawTarget, and that
  // needs another DrawTarget to be passed to it.  You might expect the type of
  // the DrawTarget that is passed to matter because it would seem logical to
  // encoded its type in the recording, and on replaying the recording a
  // DrawTarget of the same type would be created.  However, the passed
  // DrawTarget's type doesn't seem to be encoded any more accurately than just
  // "BackendType::CAIRO".  Even if it were, the code that replays the
  // recording is PrintTranslator::TranslateRecording which (indirectly) calls
  // MakePrintTarget on the type of nsIDeviceContextSpecProxy that is created
  // for the platform that we're running on, and the type of DrawTarget that
  // that returns is hardcoded.
  //
  // The only reason that we use cairo_recording_surface_create here is:
  //
  //   * It's pretty much the only cairo_*_surface_create methods that's both
  //     available on all platforms and doesn't require allocating a
  //     potentially large surface.
  //
  //   * Since we need a DrawTarget to pass to CreateRecordingDrawTarget we
  //     might as well leverage our base class's machinery to create a
  //     DrawTarget (it's as good a way as any other that will work), and to do
  //     that we need a cairo_surface_t.
  //
  // So the fact that this is a "recording" PrintTarget and the function that
  // we call here is cairo_recording_surface_create is simply a coincidence. We
  // could use any cairo_*_surface_create method and this class would still
  // work.
  //
  cairo_surface_t* surface =
    cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, nullptr);

  if (cairo_surface_status(surface)) {
    return nullptr;
  }

  // The new object takes ownership of our surface reference.
  RefPtr<PrintTargetRecording> target =
    new PrintTargetRecording(surface, aSize);

  return target.forget();
}

already_AddRefed<DrawTarget>
PrintTargetRecording::MakeDrawTarget(const IntSize& aSize,
                                     DrawEventRecorder* aRecorder)
{
  MOZ_ASSERT(aRecorder, "A DrawEventRecorder is required");

  if (!aRecorder) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt = PrintTarget::MakeDrawTarget(aSize, nullptr);
  if (dt) {
    dt = CreateRecordingDrawTarget(aRecorder, dt);
    if (!dt || !dt->IsValid()) {
      return nullptr;
    }
  }

  return dt.forget();
}

already_AddRefed<DrawTarget>
PrintTargetRecording::CreateRecordingDrawTarget(DrawEventRecorder* aRecorder,
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

} // namespace gfx
} // namespace mozilla
