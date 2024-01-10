/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERDRAWTARGETRECORDER_H
#define MOZILLA_LAYERS_WEBRENDERDRAWTARGETRECORDER_H

#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/InlineTranslator.h"
#include "mozilla/webrender/webrender_ffi.h"

namespace mozilla {
namespace layers {

struct BlobFont {
  wr::FontInstanceKey mFontInstanceKey;
  gfx::ReferencePtr mScaledFontPtr;
};

class WebRenderDrawEventRecorder final : public gfx::DrawEventRecorderMemory {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebRenderDrawEventRecorder, final)

  explicit WebRenderDrawEventRecorder(
      const gfx::SerializeResourcesFn& aSerialize)
      : DrawEventRecorderMemory(aSerialize) {}

  gfx::RecorderType GetRecorderType() const final {
    return gfx::RecorderType::WEBRENDER;
  }

  void StoreSourceSurfaceRecording(gfx::SourceSurface* aSurface,
                                   const char* aReason) final;

 private:
  virtual ~WebRenderDrawEventRecorder() = default;
};

class WebRenderTranslator final : public gfx::InlineTranslator {
 public:
  explicit WebRenderTranslator(gfx::DrawTarget* aDT,
                               void* aFontContext = nullptr)
      : InlineTranslator(aDT, aFontContext) {}

  already_AddRefed<gfx::SourceSurface> LookupExternalSurface(
      uint64_t aKey) final;
};

}  // namespace layers
}  // namespace mozilla

#endif
