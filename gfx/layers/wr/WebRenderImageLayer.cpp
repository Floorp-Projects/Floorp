/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageLayer.h"

#include "LayersLogging.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace layers {

using namespace gfx;

already_AddRefed<gfx::SourceSurface>
WebRenderImageLayer::GetAsSourceSurface()
{
  AutoLockImage autoLock(mContainer);
  Image *image = autoLock.GetImage();
  if (!image) {
    return nullptr;
  }
  RefPtr<gfx::SourceSurface> surface = image->GetAsSourceSurface();
  if (!surface || !surface->IsValid()) {
    return nullptr;
  }
  return surface.forget();
}

void
WebRenderImageLayer::RenderLayer()
{
  RefPtr<gfx::SourceSurface> surface = GetAsSourceSurface();
  if (!surface)
    return;

  WRScrollFrameStackingContextGenerator scrollFrames(this);

  RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
  DataSourceSurface::ScopedMap map(dataSurface, DataSourceSurface::MapType::READ);
  //XXX
  MOZ_RELEASE_ASSERT(surface->GetFormat() == SurfaceFormat::B8G8R8X8 ||
                     surface->GetFormat() == SurfaceFormat::B8G8R8A8, "bad format");

  gfx::IntSize size = surface->GetSize();

  WRImageKey key;
  gfx::ByteBuffer buf(size.height * map.GetStride(), map.GetData());
  WRBridge()->SendAddImage(size.width, size.height, map.GetStride(), RGBA8, buf, &key);

  Rect rect(0, 0, size.width, size.height);

  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }
  if (gfxPrefs::LayersDump()) printf_stderr("ImageLayer %p using rect:%s clip:%s\n", this, Stringify(rect).c_str(), Stringify(clip).c_str());
  WRBridge()->AddWebRenderCommand(OpPushDLBuilder());
  WRBridge()->AddWebRenderCommand(OpDPPushImage(toWrRect(rect), toWrRect(clip), Nothing(), key));
  Manager()->AddImageKeyForDiscard(key);

  Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  Matrix4x4 transform;// = GetTransform();
  if (gfxPrefs::LayersDump()) printf_stderr("ImageLayer %p using %s as bounds/overflow, %s for transform\n", this, Stringify(relBounds).c_str(), Stringify(transform).c_str());
  WRBridge()->AddWebRenderCommand(
    OpPopDLBuilder(toWrRect(relBounds), toWrRect(relBounds), transform, FrameMetrics::NULL_SCROLL_ID));

  //mContainer->SetImageFactory(originalIF);
}

} // namespace layers
} // namespace mozilla
