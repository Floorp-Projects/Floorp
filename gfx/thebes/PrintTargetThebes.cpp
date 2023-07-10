/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetThebes.h"

#include "gfxASurface.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla::gfx {

/* static */
already_AddRefed<PrintTargetThebes> PrintTargetThebes::CreateOrNull(
    gfxASurface* aSurface) {
  MOZ_ASSERT(aSurface);

  if (!aSurface || aSurface->CairoStatus()) {
    return nullptr;
  }

  RefPtr<PrintTargetThebes> target = new PrintTargetThebes(aSurface);

  return target.forget();
}

PrintTargetThebes::PrintTargetThebes(gfxASurface* aSurface)
    : PrintTarget(nullptr, aSurface->GetSize()), mGfxSurface(aSurface) {}

already_AddRefed<DrawTarget> PrintTargetThebes::MakeDrawTarget(
    const IntSize& aSize, DrawEventRecorder* aRecorder) {
  // This should not be called outside of BeginPage()/EndPage() calls since
  // some backends can only provide a valid DrawTarget at that time.
  MOZ_ASSERT(mHasActivePage, "We can't guarantee a valid DrawTarget");

  RefPtr<gfx::DrawTarget> dt =
      gfxPlatform::CreateDrawTargetForSurface(mGfxSurface, aSize);
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

already_AddRefed<DrawTarget> PrintTargetThebes::GetReferenceDrawTarget() {
  if (!mRefDT) {
    RefPtr<gfx::DrawTarget> dt =
        gfxPlatform::CreateDrawTargetForSurface(mGfxSurface, mSize);
    if (!dt || !dt->IsValid()) {
      return nullptr;
    }
    mRefDT = dt->CreateSimilarDrawTarget(IntSize(1, 1), dt->GetFormat());
  }

  return do_AddRef(mRefDT);
}

nsresult PrintTargetThebes::BeginPrinting(const nsAString& aTitle,
                                          const nsAString& aPrintToFileName,
                                          int32_t aStartPage,
                                          int32_t aEndPage) {
  return mGfxSurface->BeginPrinting(aTitle, aPrintToFileName);
}

nsresult PrintTargetThebes::EndPrinting() { return mGfxSurface->EndPrinting(); }

nsresult PrintTargetThebes::AbortPrinting() {
#ifdef DEBUG
  mHasActivePage = false;
#endif
  return mGfxSurface->AbortPrinting();
}

nsresult PrintTargetThebes::BeginPage(const IntSize& aSizeInPoints) {
#ifdef DEBUG
  mHasActivePage = true;
#endif
  return mGfxSurface->BeginPage();
}

nsresult PrintTargetThebes::EndPage() {
#ifdef DEBUG
  mHasActivePage = false;
#endif
  return mGfxSurface->EndPage();
}

void PrintTargetThebes::Finish() { return mGfxSurface->Finish(); }

}  // namespace mozilla::gfx
