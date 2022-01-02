/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_RECORDEDFRAME_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_RECORDEDFRAME_H

#include "mozilla/layers/CompositionRecorder.h"

namespace mozilla {

namespace wr {

class RenderCompositorRecordedFrame final : public layers::RecordedFrame {
 public:
  RenderCompositorRecordedFrame(
      const TimeStamp& aTimeStamp,
      RefPtr<layers::profiler_screenshots::AsyncReadbackBuffer>&& aBuffer)
      : RecordedFrame(aTimeStamp), mBuffer(aBuffer) {}

  virtual already_AddRefed<gfx::DataSourceSurface> GetSourceSurface() override {
    if (mSurface) {
      return do_AddRef(mSurface);
    }

    gfx::IntSize size = mBuffer->Size();
    mSurface = gfx::Factory::CreateDataSourceSurface(
        size, gfx::SurfaceFormat::B8G8R8A8,
        /* aZero = */ false);

    if (!mBuffer->MapAndCopyInto(mSurface, size)) {
      mSurface = nullptr;
      return nullptr;
    }

    return do_AddRef(mSurface);
  }

 private:
  RefPtr<layers::profiler_screenshots::AsyncReadbackBuffer> mBuffer;
  RefPtr<gfx::DataSourceSurface> mSurface;
};

}  // namespace wr
}  // namespace mozilla

#endif
