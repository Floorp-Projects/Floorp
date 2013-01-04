/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CANVASIMAGECACHE_H_
#define CANVASIMAGECACHE_H_

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsHTMLCanvasElement;
class imgIRequest;
class gfxASurface;

#include "gfxPoint.h"

namespace mozilla {

class CanvasImageCache {
public:
  /**
   * Notify that image element aImage was (or is about to be) drawn to aCanvas
   * using the first frame of aRequest's image. The data for the surface is
   * in aSurface, and the image size is in aSize.
   */
  static void NotifyDrawImage(dom::Element* aImage,
                              nsHTMLCanvasElement* aCanvas,
                              imgIRequest* aRequest,
                              gfxASurface* aSurface,
                              const gfxIntSize& aSize);

  /**
   * Check whether aImage has recently been drawn into aCanvas. If we return
   * a non-null surface, then the image was recently drawn into the canvas
   * (with the same image request) and the returned surface contains the image
   * data, and the image size will be returned in aSize.
   */
  static gfxASurface* Lookup(dom::Element* aImage,
                             nsHTMLCanvasElement* aCanvas,
                             gfxIntSize* aSize);
};

}

#endif /* CANVASIMAGECACHE_H_ */
