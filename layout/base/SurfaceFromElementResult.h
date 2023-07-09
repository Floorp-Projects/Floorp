/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SurfaceFromElementResult_h
#define mozilla_SurfaceFromElementResult_h

#include "ImageContainer.h"
#include "gfxTypes.h"
#include "mozilla/gfx/Point.h"
#include "nsCOMPtr.h"
#include <cstdint>

class imgIContainer;
class imgIRequest;
class nsIPrincipal;
class nsLayoutUtils;

namespace mozilla {

namespace dom {
class CanvasRenderingContext2D;
class ImageBitmap;
}  // namespace dom

namespace gfx {
class SourceSurface;
}

struct DirectDrawInfo {
  /* imgIContainer to directly draw to a context */
  nsCOMPtr<imgIContainer> mImgContainer;
  /* which frame to draw */
  uint32_t mWhichFrame;
  /* imgIContainer flags to use when drawing */
  uint32_t mDrawingFlags;
};

struct SurfaceFromElementResult {
  friend class mozilla::dom::CanvasRenderingContext2D;
  friend class mozilla::dom::ImageBitmap;
  friend class ::nsLayoutUtils;

  /* If SFEResult contains a valid surface, it either mLayersImage or
   * mSourceSurface will be non-null, and GetSourceSurface() will not be null.
   *
   * For valid surfaces, mSourceSurface may be null if mLayersImage is
   * non-null, but GetSourceSurface() will create mSourceSurface from
   * mLayersImage when called.
   */

  /* Video elements (at least) often are already decoded as layers::Images. */
  RefPtr<mozilla::layers::Image> mLayersImage;

 protected:
  /* GetSourceSurface() fills this and returns its non-null value if this
   * SFEResult was successful. */
  RefPtr<mozilla::gfx::SourceSurface> mSourceSurface;

 public:
  /* Contains info for drawing when there is no mSourceSurface. */
  DirectDrawInfo mDrawInfo;

  /* The size of the surface */
  mozilla::gfx::IntSize mSize;
  /* The size the surface is intended to be rendered at */
  mozilla::gfx::IntSize mIntrinsicSize;
  /* The crop rect of the surface, indicating what subset is valid. This will
   * always be Nothing() unless SFE_ALLOW_UNCROPPED is set. */
  mozilla::Maybe<mozilla::gfx::IntRect> mCropRect;
  /* The principal associated with the element whose surface was returned.
     If there is a surface, this will never be null. */
  nsCOMPtr<nsIPrincipal> mPrincipal;
  /* The image request, if the element is an nsIImageLoadingContent */
  nsCOMPtr<imgIRequest> mImageRequest;
  /* True if cross-origins redirects have been done in order to load this
   * resource */
  bool mHadCrossOriginRedirects;
  /* Whether the element was "write only", that is, the bits should not be
   * exposed to content */
  bool mIsWriteOnly;
  /* Whether the element was still loading.  Some consumers need to handle
     this case specially. */
  bool mIsStillLoading;
  /* Whether the element has a valid size. */
  bool mHasSize;
  /* Whether the element used CORS when loading. */
  bool mCORSUsed;

  gfxAlphaType mAlphaType;

  // Methods:

  SurfaceFromElementResult();

  // Gets mSourceSurface, or makes a SourceSurface from mLayersImage.
  const RefPtr<mozilla::gfx::SourceSurface>& GetSourceSurface();
};

}  // namespace mozilla

#endif  // mozilla_SurfaceFromElementResult_h
