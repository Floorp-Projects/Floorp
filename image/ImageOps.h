/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageOps_h
#define mozilla_image_ImageOps_h

#include "nsCOMPtr.h"
#include "nsRect.h"

class gfxDrawable;
class imgIContainer;
class nsIInputStream;

namespace mozilla {

namespace gfx {
class SourceSurface;
}

namespace image {

class Image;
struct Orientation;

class ImageOps
{
public:
  /**
   * Creates a version of an existing image which does not animate and is frozen
   * at the first frame.
   *
   * @param aImage         The existing image.
   */
  static already_AddRefed<Image> Freeze(Image* aImage);
  static already_AddRefed<imgIContainer> Freeze(imgIContainer* aImage);

  /**
   * Creates a clipped version of an existing image. Animation is unaffected.
   *
   * @param aImage           The existing image.
   * @param aClip            The rectangle to clip the image against.
   * @param aSVGViewportSize The specific viewort size of aImage. Unless aImage
   *                         is a vector image without intrinsic size, this
   *                         argument should be pass as Nothing().
   */
  static already_AddRefed<Image> Clip(Image* aImage, nsIntRect aClip,
                                      const Maybe<nsSize>& aSVGViewportSize =
                                        Nothing());
  static already_AddRefed<imgIContainer> Clip(imgIContainer* aImage,
                                              nsIntRect aClip,
                                              const Maybe<nsSize>& aSVGViewportSize =
                                                Nothing());

  /**
   * Creates a version of an existing image which is rotated and/or flipped to
   * the specified orientation.
   *
   * @param aImage         The existing image.
   * @param aOrientation   The desired orientation.
   */
  static already_AddRefed<Image> Orient(Image* aImage,
                                        Orientation aOrientation);
  static already_AddRefed<imgIContainer> Orient(imgIContainer* aImage,
                                                Orientation aOrientation);

  /**
   * Creates an image from a gfxDrawable.
   *
   * @param aDrawable      The gfxDrawable.
   */
  static already_AddRefed<imgIContainer>
  CreateFromDrawable(gfxDrawable* aDrawable);

  /**
   * Decodes an image from an nsIInputStream directly into a SourceSurface,
   * without ever creating an Image or imgIContainer (which are mostly
   * main-thread-only). That means that this function may be called
   * off-main-thread.
   *
   * @param aInputStream An input stream containing an encoded image.
   * @param aMimeType The MIME type of the image.
   * @param aFlags Flags of the imgIContainer::FLAG_DECODE_* variety.
   * @return A SourceSurface containing the first frame of the image at its
   *         intrinsic size, or nullptr if the image cannot be decoded.
   */
  static already_AddRefed<gfx::SourceSurface>
  DecodeToSurface(nsIInputStream* aInputStream,
                  const nsACString& aMimeType,
                  uint32_t aFlags);

private:
  // This is a static utility class, so disallow instantiation.
  virtual ~ImageOps() = 0;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_ImageOps_h
