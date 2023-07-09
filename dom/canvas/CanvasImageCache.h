/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CANVASIMAGECACHE_H_
#define CANVASIMAGECACHE_H_

#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Rect.h"
#include "nsSize.h"

namespace mozilla {
namespace dom {
class Element;
class HTMLCanvasElement;
}  // namespace dom
namespace gfx {
class DrawTarget;
class SourceSurface;
}  // namespace gfx
}  // namespace mozilla
class imgIContainer;

namespace mozilla {

class CanvasImageCache {
  using SourceSurface = mozilla::gfx::SourceSurface;

 public:
  /**
   * Notify that image element aImage was drawn to aCanvas element
   * using the first frame of aRequest's image. The data for the surface is
   * in aSurface, and the image size is in aSize. aIntrinsicSize is the size
   * the surface is intended to be rendered at.
   */
  static void NotifyDrawImage(dom::Element* aImage,
                              dom::HTMLCanvasElement* aCanvas,
                              gfx::DrawTarget* aTarget, SourceSurface* aSource,
                              const gfx::IntSize& aSize,
                              const gfx::IntSize& aIntrinsicSize,
                              const Maybe<gfx::IntRect>& aCropRect);

  /**
   * Check whether aImage has recently been drawn any canvas. If we return
   * a non-null surface, then the same image was recently drawn into a canvas.
   */
  static SourceSurface* LookupAllCanvas(dom::Element* aImage,
                                        gfx::DrawTarget* aTarget);

  /**
   * Like the top above, but restricts the lookup to only aCanvas. This is
   * required for CORS security.
   */
  static SourceSurface* LookupCanvas(dom::Element* aImage,
                                     dom::HTMLCanvasElement* aCanvas,
                                     gfx::DrawTarget* aTarget,
                                     gfx::IntSize* aSizeOut,
                                     gfx::IntSize* aIntrinsicSizeOut,
                                     Maybe<gfx::IntRect>* aCropRectOut);
};

}  // namespace mozilla

#endif /* CANVASIMAGECACHE_H_ */
