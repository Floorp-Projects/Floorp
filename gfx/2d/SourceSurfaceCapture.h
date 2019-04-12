/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_2d_SourceSurfaceCapture_h
#define mozilla_gfx_2d_SourceSurfaceCapture_h

#include "2D.h"
#include "CaptureCommandList.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace gfx {

class DrawTargetCaptureImpl;

class SourceSurfaceCapture : public SourceSurface {
  friend class DrawTargetCaptureImpl;

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceCapture, override)

  explicit SourceSurfaceCapture(DrawTargetCaptureImpl* aOwner);
  SourceSurfaceCapture(DrawTargetCaptureImpl* aOwner,
                       LuminanceType aLuminanceType, float aOpacity);
  SourceSurfaceCapture(DrawTargetCaptureImpl* aOwner,
                       SourceSurface* aSurfToOptimize);
  virtual ~SourceSurfaceCapture();

  SurfaceType GetType() const override { return SurfaceType::CAPTURE; }
  IntSize GetSize() const override { return mSize; }
  SurfaceFormat GetFormat() const override { return mFormat; }

  bool IsValid() const override;
  already_AddRefed<DataSourceSurface> GetDataSurface() override;

  // The backend hint is not guaranteed to be honored, so callers must check
  // the resulting type if needed.
  RefPtr<SourceSurface> Resolve(BackendType aBackendType = BackendType::NONE);

 protected:
  RefPtr<SourceSurface> ResolveImpl(BackendType aBackendType);
  void DrawTargetWillDestroy();
  void DrawTargetWillChange();

 private:
  IntSize mSize;
  SurfaceFormat mFormat;
  int32_t mStride;
  int32_t mSurfaceAllocationSize;
  RefPtr<DrawTarget> mRefDT;
  DrawTargetCaptureImpl* mOwner;
  CaptureCommandList mCommands;
  bool mHasCommandList;

  bool mShouldResolveToLuminance;
  LuminanceType mLuminanceType;
  float mOpacity;

  // Note that we have to keep a reference around. Internal methods like
  // GetSkImageForSurface expect their callers to hold a reference, which
  // isn't easily possible for nested surfaces.
  mutable Mutex mLock;
  RefPtr<SourceSurface> mResolved;
  RefPtr<SourceSurface> mSurfToOptimize;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_2d_SourceSurfaceCapture_h
