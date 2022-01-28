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
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {
class HTMLCanvasElement;

struct OffscreenCanvasDisplayData final {
  mozilla::gfx::IntSize mSize = {0, 0};
  bool mDoPaintCallbacks = false;
  bool mIsOpaque = true;
  bool mIsAlphaPremult = true;
  mozilla::gl::OriginPos mOriginPos = gl::OriginPos::TopLeft;
};

class OffscreenCanvasDisplayHelper final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OffscreenCanvasDisplayHelper)

 public:
  explicit OffscreenCanvasDisplayHelper(HTMLCanvasElement* aCanvasElement,
                                        uint32_t aWidth, uint32_t aHeight);

  CanvasContextType GetContextType() const;

  RefPtr<layers::ImageContainer> GetImageContainer() const;

  void UpdateContext(CanvasContextType aType, int32_t aChildId);

  bool CommitFrameToCompositor(nsICanvasRenderingContextInternal* aContext,
                               layers::TextureType aTextureType,
                               const Maybe<OffscreenCanvasDisplayData>& aData);

  void Destroy();

  already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot();
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

  OffscreenCanvasDisplayData mData;
  CanvasContextType mType = CanvasContextType::NoContext;
  uint32_t mContextManagerId = 0;
  int32_t mContextChildId = 0;
  mozilla::layers::ImageContainer::ProducerID mImageProducerID;
  mozilla::layers::ImageContainer::FrameID mLastFrameID = 0;
  bool mPendingInvalidate = false;
};

}  // namespace mozilla::dom

#endif  // MOZILLA_DOM_OFFSCREENCANVASDISPLAYHELPER_H_
