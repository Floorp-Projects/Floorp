/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_
#define MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_

#include "CanvasRenderingContextHelper.h"
#include "ImageContainer.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {
class HTMLCanvasElement;

class OffscreenCanvasDisplayHelper final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OffscreenCanvasDisplayHelper)

 public:
  explicit OffscreenCanvasDisplayHelper(HTMLCanvasElement* aCanvasElement,
                                        uint32_t aWidth, uint32_t aHeight);

  CanvasContextType GetContextType() const;

  RefPtr<layers::ImageContainer> GetImageContainer() const;

  void UpdateContext(layers::ImageContainer* aContainer,
                     CanvasContextType aType, int32_t aChildId);

  void UpdateParameters(uint32_t aWidth, uint32_t aHeight, bool aHasAlpha,
                        bool aIsPremultiplied, bool aIsOriginBottomLeft);

  bool CommitFrameToCompositor(nsICanvasRenderingContextInternal* aContext,
                               layers::TextureType aTextureType);

  void Destroy();

  mozilla::Maybe<mozilla::layers::SurfaceDescriptor> GetFrontBuffer(
      mozilla::WebGLFramebufferJS*, const bool webvr = false);

  already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(
      gfxAlphaType* out_alphaType = nullptr);

  already_AddRefed<mozilla::layers::Image> GetAsImage();

 private:
  ~OffscreenCanvasDisplayHelper();
  void MaybeQueueInvalidateElement();
  void InvalidateElement();

  mutable Mutex mMutex;
  HTMLCanvasElement* MOZ_NON_OWNING_REF mCanvasElement;
  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<gfx::SourceSurface> mFrontBufferSurface;
  Maybe<layers::SurfaceDescriptor> mFrontBufferDesc;

  CanvasContextType mType = CanvasContextType::NoContext;
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mContextManagerId = 0;
  int32_t mContextChildId = 0;
  mozilla::layers::ImageContainer::ProducerID mImageProducerID;
  mozilla::layers::ImageContainer::FrameID mLastFrameID = 0;
  bool mPendingInvalidate = false;
  bool mHasAlpha = false;
  bool mIsPremultiplied = false;
  bool mIsOriginBottomLeft = false;
};

}  // namespace mozilla::dom

#endif  // MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_
