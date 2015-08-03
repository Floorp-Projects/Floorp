/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageMetadata.h"

#include "RasterImage.h"
#include "nsComponentManagerUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsXPCOMCID.h"

namespace mozilla {
namespace image {

nsresult
ImageMetadata::SetOnImage(RasterImage* aImage)
{
  nsresult rv = NS_OK;

  if (mHotspotX != -1 && mHotspotY != -1) {
    nsCOMPtr<nsISupportsPRUint32> intwrapx =
      do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID);
    nsCOMPtr<nsISupportsPRUint32> intwrapy =
      do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID);
    intwrapx->SetData(mHotspotX);
    intwrapy->SetData(mHotspotY);
    aImage->Set("hotspotX", intwrapx);
    aImage->Set("hotspotY", intwrapy);
  }

  aImage->SetLoopCount(mLoopCount);

  if (HasSize()) {
    MOZ_ASSERT(HasOrientation(), "Should have orientation");
    rv = aImage->SetSize(GetWidth(), GetHeight(), GetOrientation());
  }

  return rv;
}

} // namespace image
} // namespace mozilla
