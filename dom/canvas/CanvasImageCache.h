/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CANVASIMAGECACHE_H_
#define CANVASIMAGECACHE_H_

#include "nsSize.h"

namespace mozilla {
namespace dom {
class Element;
class HTMLCanvasElement;
} // namespace dom
namespace gfx {
class SourceSurface;
} // namespace gfx
} // namespace mozilla
class imgIRequest;

namespace mozilla {

class CanvasImageCache {
  typedef mozilla::gfx::SourceSurface SourceSurface;
public:
  /**
   * Notify that image element aImage was (or is about to be) drawn to aCanvas
   * using the first frame of aRequest's image. The data for the surface is
   * in aSurface, and the image size is in aSize.
   */
  static void NotifyDrawImage(dom::Element* aImage,
                              dom::HTMLCanvasElement* aCanvas,
                              imgIRequest* aRequest,
                              SourceSurface* aSource,
                              const gfx::IntSize& aSize);

  /**
   * Check whether aImage has recently been drawn into aCanvas. If we return
   * a non-null surface, then the image was recently drawn into the canvas
   * (with the same image request) and the returned surface contains the image
   * data, and the image size will be returned in aSize.
   */
  static SourceSurface* Lookup(dom::Element* aImage,
                               dom::HTMLCanvasElement* aCanvas,
                               gfx::IntSize* aSize);

  /**
   * This is the same as Lookup, except it works on any image recently drawn
   * into any canvas. Security checks need to be done again if using the
   * results from this.
   */
  static SourceSurface* SimpleLookup(dom::Element* aImage);
};

} // namespace mozilla

#endif /* CANVASIMAGECACHE_H_ */
