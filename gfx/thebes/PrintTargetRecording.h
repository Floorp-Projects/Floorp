/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETRECORDING_H
#define MOZILLA_GFX_PRINTTARGETRECORDING_H

#include "PrintTarget.h"

namespace mozilla {
namespace gfx {

/**
 * Recording printing target.
 *
 * This exists for use on e10s's content process in order to record print
 * output, send it over to the parent process, and replay it on a DrawTarget
 * there for printing.
 */
class PrintTargetRecording final : public PrintTarget
{
public:
  static already_AddRefed<PrintTargetRecording>
  CreateOrNull(const IntSize& aSize);

  virtual already_AddRefed<DrawTarget>
  MakeDrawTarget(const IntSize& aSize,
                 DrawEventRecorder* aRecorder = nullptr) override;

private:
  PrintTargetRecording(cairo_surface_t* aCairoSurface,
                       const IntSize& aSize);

  already_AddRefed<DrawTarget>
  CreateWrapAndRecordDrawTarget(DrawEventRecorder* aRecorder,
                                DrawTarget* aDrawTarget);
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETRECORDING_H */
