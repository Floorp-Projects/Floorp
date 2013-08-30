/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgIContainer.h"
#include "ClippedImage.h"
#include "FrozenImage.h"
#include "OrientedImage.h"
#include "Image.h"

#include "ImageOps.h"

namespace mozilla {
namespace image {

/* static */ already_AddRefed<Image>
ImageOps::Freeze(Image* aImage)
{
  nsRefPtr<Image> frozenImage = new FrozenImage(aImage);
  return frozenImage.forget();
}

/* static */ already_AddRefed<imgIContainer>
ImageOps::Freeze(imgIContainer* aImage)
{
  nsCOMPtr<imgIContainer> frozenImage =
    new FrozenImage(static_cast<Image*>(aImage));
  return frozenImage.forget();
}

/* static */ already_AddRefed<Image>
ImageOps::Clip(Image* aImage, nsIntRect aClip)
{
  nsRefPtr<Image> clippedImage = new ClippedImage(aImage, aClip);
  return clippedImage.forget();
}

/* static */ already_AddRefed<imgIContainer>
ImageOps::Clip(imgIContainer* aImage, nsIntRect aClip)
{
  nsCOMPtr<imgIContainer> clippedImage =
    new ClippedImage(static_cast<Image*>(aImage), aClip);
  return clippedImage.forget();
}

/* static */ already_AddRefed<Image>
ImageOps::Orient(Image* aImage, Orientation aOrientation)
{
  nsRefPtr<Image> orientedImage = new OrientedImage(aImage, aOrientation);
  return orientedImage.forget();
}

/* static */ already_AddRefed<imgIContainer>
ImageOps::Orient(imgIContainer* aImage, Orientation aOrientation)
{
  nsCOMPtr<imgIContainer> orientedImage =
    new OrientedImage(static_cast<Image*>(aImage), aOrientation);
  return orientedImage.forget();
}

} // namespace image
} // namespace mozilla
