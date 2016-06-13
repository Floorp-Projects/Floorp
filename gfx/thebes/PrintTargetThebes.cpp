/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTargetThebes.h"

#include "gfxASurface.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

/* static */ already_AddRefed<PrintTargetThebes>
PrintTargetThebes::CreateOrNull(gfxASurface* aSurface)
{
  MOZ_ASSERT(aSurface);

  if (!aSurface || aSurface->CairoStatus()) {
    return nullptr;
  }

  RefPtr<PrintTargetThebes> target = new PrintTargetThebes(aSurface);

  return target.forget();
}

PrintTargetThebes::PrintTargetThebes(gfxASurface* aSurface)
  : PrintTarget(nullptr, aSurface->GetSize())
  , mGfxSurface(aSurface)
{
}

already_AddRefed<DrawTarget>
PrintTargetThebes::MakeDrawTarget(const IntSize& aSize,
                                  DrawEventRecorder* aRecorder)
{
  MOZ_ASSERT(!aRecorder,
             "aRecorder should only be passed to an instance of "
             "PrintTargetRecording");

  RefPtr<gfx::DrawTarget> dt =
    gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mGfxSurface, aSize);
  if (!dt || !dt->IsValid()) {
    return nullptr;
  }

  return dt.forget();
}

nsresult
PrintTargetThebes::BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName)
{
  return mGfxSurface->BeginPrinting(aTitle, aPrintToFileName);
}

nsresult
PrintTargetThebes::EndPrinting()
{
  return mGfxSurface->EndPrinting();
}

nsresult
PrintTargetThebes::AbortPrinting()
{
  return mGfxSurface->AbortPrinting();
}

nsresult
PrintTargetThebes::BeginPage()
{
  return mGfxSurface->BeginPage();
}

nsresult
PrintTargetThebes::EndPage()
{
  return mGfxSurface->EndPage();
}

void
PrintTargetThebes::Finish()
{
  return mGfxSurface->Finish();
}

} // namespace gfx
} // namespace mozilla
