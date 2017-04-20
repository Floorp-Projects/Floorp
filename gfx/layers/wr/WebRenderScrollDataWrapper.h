/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERSCROLLDATAWRAPPER_H
#define GFX_WEBRENDERSCROLLDATAWRAPPER_H

#include "FrameMetrics.h"
#include "mozilla/layers/WebRenderScrollData.h"

namespace mozilla {
namespace layers {

/*
 * This class is a wrapper to walk through a WebRenderScrollData object, with
 * an exposed API that is template-compatible to LayerMetricsWrapper. This allows
 * APZ to walk through both layer trees and WebRender scroll metadata structures
 * without a lot of code duplication.
 * (Note that not all functions from LayerMetricsWrapper are implemented here,
 * only the ones we've needed in APZ code so far.)
 *
 * One important note here is that this class holds a pointer to the "owning"
 * WebRenderScrollData. The caller must ensure that this class does not outlive
 * the owning WebRenderScrollData, or this may result in use-after-free errors.
 * This class being declared a MOZ_STACK_CLASS should help with that.
 *
 * Refer to LayerMetricsWrapper.h for actual documentation on the exposed API.
 */
class MOZ_STACK_CLASS WebRenderScrollDataWrapper {
public:
  explicit WebRenderScrollDataWrapper(const WebRenderScrollData* aData)
    : mData(aData)
  {
  }

  explicit operator bool() const
  {
    // TODO
    return false;
  }

  bool IsScrollInfoLayer() const
  {
    // TODO
    return false;
  }

  WebRenderScrollDataWrapper GetLastChild() const
  {
    // TODO
    return WebRenderScrollDataWrapper(nullptr);
  }

  WebRenderScrollDataWrapper GetPrevSibling() const
  {
    // TODO
    return WebRenderScrollDataWrapper(nullptr);
  }

  const ScrollMetadata& Metadata() const
  {
    // TODO
    return *ScrollMetadata::sNullMetadata;
  }

  const FrameMetrics& Metrics() const
  {
    return Metadata().GetMetrics();
  }

  AsyncPanZoomController* GetApzc() const
  {
    // TODO
    return nullptr;
  }

  void SetApzc(AsyncPanZoomController* aApzc) const
  {
    // TODO
  }

  const char* Name() const
  {
    // TODO
    return nullptr;
  }

  gfx::Matrix4x4 GetTransform() const
  {
    // TODO
    return gfx::Matrix4x4();
  }

  CSSTransformMatrix GetTransformTyped() const
  {
    return ViewAs<CSSTransformMatrix>(GetTransform());
  }

  bool TransformIsPerspective() const
  {
    // TODO
    return false;
  }

  EventRegions GetEventRegions() const
  {
    // TODO
    return EventRegions();
  }

  Maybe<uint64_t> GetReferentId() const
  {
    // TODO
    return Nothing();
  }

  Maybe<ParentLayerIntRect> GetClipRect() const
  {
    // TODO
    return Nothing();
  }

  EventRegionsOverride GetEventRegionsOverride() const
  {
    // TODO
    return EventRegionsOverride::NoOverride;
  }

  ScrollDirection GetScrollbarDirection() const
  {
    // TODO
    return ScrollDirection::NONE;
  }

  FrameMetrics::ViewID GetScrollbarTargetContainerId() const
  {
    // TODO
    return 0;
  }

  int32_t GetScrollThumbLength() const
  {
    // TODO
    return 0;
  }

  bool IsScrollbarContainer() const
  {
    // TODO
    return false;
  }

  FrameMetrics::ViewID GetFixedPositionScrollContainerId() const
  {
    // TODO
    return 0;
  }

  const void* GetLayer() const
  {
    // TODO
    return nullptr;
  }

private:
  const WebRenderScrollData* mData;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERSCROLLDATAWRAPPER_H */
