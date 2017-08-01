/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_UPDATEIMAGEHELPER_H
#define GFX_UPDATEIMAGEHELPER_H

#include "Layers.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace layers {

class UpdateImageHelper
{
public:
  UpdateImageHelper(ImageContainer* aImageContainer,
                    ImageClient* aImageClient,
                    gfx::IntSize aImageSize,
                    gfx::SurfaceFormat aFormat) :
    mImageContainer(aImageContainer),
    mImageClient(aImageClient),
    mImageSize(aImageSize),
    mIsLocked(false)
  {
    mTexture = mImageClient->GetTextureClientRecycler()->CreateOrRecycle(aFormat,
                                                                         mImageSize,
                                                                         BackendSelector::Content,
                                                                         TextureFlags::DEFAULT);
    if (!mTexture) {
      return;
    }

    mIsLocked = mTexture->Lock(OpenMode::OPEN_WRITE_ONLY);
    if (!mIsLocked) {
      return;
    }
  }

  ~UpdateImageHelper()
  {
    if (mIsLocked) {
      mTexture->Unlock();
      mIsLocked = false;
    }
  }

  already_AddRefed<gfx::DrawTarget> GetDrawTarget()
  {
    RefPtr<gfx::DrawTarget> target;
    if (mTexture) {
      target = mTexture->BorrowDrawTarget();
    }
    return target.forget();
  }

  bool UpdateImage()
  {
    if (!mTexture) {
      return false;
    }

    if (mIsLocked) {
      mTexture->Unlock();
      mIsLocked = false;
    }

    RefPtr<TextureWrapperImage> image = new TextureWrapperImage(mTexture,
                                                                gfx::IntRect(gfx::IntPoint(0, 0), mImageSize));
    mImageContainer->SetCurrentImageInTransaction(image);
    return mImageClient->UpdateImage(mImageContainer, /* unused */0);
  }

private:
  RefPtr<ImageContainer> mImageContainer;
  RefPtr<ImageClient> mImageClient;
  gfx::IntSize mImageSize;
  RefPtr<TextureClient> mTexture;
  bool mIsLocked;
};

} // namespace layers
} // namespace mozilla

#endif  // GFX_UPDATEIMAGEHELPER_H
