/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_
#define MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_

#include "ImageContainer.h"
#include "GLContextTypes.h"
#include "mozilla/dom/CanvasRenderingContextHelper.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {
class HTMLCanvasElement;
class OffscreenCanvas;
class ThreadSafeWorkerRef;

struct OffscreenCanvasDisplayData final {
  mozilla::gfx::IntSize mSize = {0, 0};
  bool mDoPaintCallbacks = false;
  bool mIsOpaque = true;
  bool mIsAlphaPremult = true;
  mozilla::gl::OriginPos mOriginPos = gl::OriginPos::TopLeft;
  Maybe<layers::RemoteTextureOwnerId> mOwnerId;
};

class OffscreenCanvasDisplayHelper final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OffscreenCanvasDisplayHelper)

 public:
  explicit OffscreenCanvasDisplayHelper(HTMLCanvasElement* aCanvasElement,
                                        uint32_t aWidth, uint32_t aHeight);

  CanvasContextType GetContextType() const;

  RefPtr<layers::ImageContainer> GetImageContainer() const;

  void UpdateContext(OffscreenCanvas* aOffscreenCanvas,
                     RefPtr<ThreadSafeWorkerRef>&& aWorkerRef,
                     CanvasContextType aType, const Maybe<int32_t>& aChildId);

  void FlushForDisplay();

  bool CommitFrameToCompositor(nsICanvasRenderingContextInternal* aContext,
                               layers::TextureType aTextureType,
                               const Maybe<OffscreenCanvasDisplayData>& aData);

  void DestroyCanvas();
  void DestroyElement();

  already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot();
  already_AddRefed<mozilla::layers::Image> GetAsImage();
  UniquePtr<uint8_t[]> GetImageBuffer(int32_t* aOutFormat,
                                      gfx::IntSize* aOutImageSize);

 private:
  ~OffscreenCanvasDisplayHelper();
  void MaybeQueueInvalidateElement() MOZ_REQUIRES(mMutex);
  void InvalidateElement();

  already_AddRefed<gfx::SourceSurface> TransformSurface(
      gfx::SourceSurface* aSurface, bool aHasAlpha, bool aIsAlphaPremult,
      gl::OriginPos aOriginPos) const;

  mutable Mutex mMutex;
  HTMLCanvasElement* MOZ_NON_OWNING_REF mCanvasElement MOZ_GUARDED_BY(mMutex);
  OffscreenCanvas* MOZ_NON_OWNING_REF mOffscreenCanvas MOZ_GUARDED_BY(mMutex) =
      nullptr;
  RefPtr<layers::ImageContainer> mImageContainer MOZ_GUARDED_BY(mMutex);
  RefPtr<gfx::SourceSurface> mFrontBufferSurface MOZ_GUARDED_BY(mMutex);
  RefPtr<ThreadSafeWorkerRef> mWorkerRef MOZ_GUARDED_BY(mMutex);

  OffscreenCanvasDisplayData mData MOZ_GUARDED_BY(mMutex);
  CanvasContextType mType MOZ_GUARDED_BY(mMutex) = CanvasContextType::NoContext;
  Maybe<uint32_t> mContextManagerId MOZ_GUARDED_BY(mMutex);
  Maybe<int32_t> mContextChildId MOZ_GUARDED_BY(mMutex);
  const mozilla::layers::ImageContainer::ProducerID mImageProducerID;
  mozilla::layers::ImageContainer::FrameID mLastFrameID MOZ_GUARDED_BY(mMutex) =
      0;
  bool mPendingInvalidate MOZ_GUARDED_BY(mMutex) = false;
};

}  // namespace mozilla::dom

#endif  // MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_
