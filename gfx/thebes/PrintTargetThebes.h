/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETTHEBES_H
#define MOZILLA_GFX_PRINTTARGETTHEBES_H

#include "mozilla/gfx/PrintTarget.h"

class gfxASurface;

namespace mozilla {
namespace gfx {

/**
 * XXX Remove this class.
 *
 * This class should go away once all the logic from the gfxASurface subclasses
 * has been moved to new PrintTarget subclasses and we no longer need to
 * wrap a gfxASurface.
 *
 * When removing this class, be sure to make PrintTarget::MakeDrawTarget
 * non-virtual!
 */
class PrintTargetThebes final : public PrintTarget {
 public:
  static already_AddRefed<PrintTargetThebes> CreateOrNull(
      gfxASurface* aSurface);

  nsresult BeginPrinting(const nsAString& aTitle,
                         const nsAString& aPrintToFileName, int32_t aStartPage,
                         int32_t aEndPage) override;
  nsresult EndPrinting() override;
  nsresult AbortPrinting() override;
  nsresult BeginPage() override;
  nsresult EndPage() override;
  void Finish() override;

  already_AddRefed<DrawTarget> MakeDrawTarget(
      const IntSize& aSize, DrawEventRecorder* aRecorder = nullptr) override;

  already_AddRefed<DrawTarget> GetReferenceDrawTarget() final;

 private:
  // Only created via CreateOrNull
  explicit PrintTargetThebes(gfxASurface* aSurface);

  RefPtr<gfxASurface> mGfxSurface;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETTHEBES_H */
