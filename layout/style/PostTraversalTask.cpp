/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PostTraversalTask.h"

#include "mozilla/dom/FontFace.h"
#include "mozilla/dom/FontFaceSet.h"
#include "gfxPlatformFontList.h"
#include "gfxTextRun.h"
#include "ServoStyleSet.h"
#include "nsPresContext.h"

namespace mozilla {

using namespace dom;

void PostTraversalTask::Run() {
  switch (mType) {
    case Type::ResolveFontFaceLoadedPromise:
      static_cast<FontFace*>(mTarget)->DoResolve();
      break;

    case Type::RejectFontFaceLoadedPromise:
      static_cast<FontFace*>(mTarget)->DoReject(mResult);
      break;

    case Type::DispatchLoadingEventAndReplaceReadyPromise:
      static_cast<FontFaceSet*>(mTarget)
          ->DispatchLoadingEventAndReplaceReadyPromise();
      break;

    case Type::DispatchFontFaceSetCheckLoadingFinishedAfterDelay:
      static_cast<FontFaceSet*>(mTarget)
          ->DispatchCheckLoadingFinishedAfterDelay();
      break;

    case Type::LoadFontEntry:
      static_cast<gfxUserFontEntry*>(mTarget)->ContinueLoad();
      break;

    case Type::InitializeFamily:
      Unused << gfxPlatformFontList::PlatformFontList()->InitializeFamily(
          static_cast<fontlist::Family*>(mTarget));
      break;

    case Type::FontInfoUpdate:
      nsPresContext* pc =
          static_cast<ServoStyleSet*>(mTarget)->GetPresContext();
      if (pc) {
        pc->ForceReflowForFontInfoUpdate();
      }
      break;
  }
}

}  // namespace mozilla
