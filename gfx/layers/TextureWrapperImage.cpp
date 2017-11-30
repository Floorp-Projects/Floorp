/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TextureWrapperImage.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

TextureWrapperImage::TextureWrapperImage(TextureClient* aClient, const IntRect& aPictureRect)
 : Image(nullptr, ImageFormat::TEXTURE_WRAPPER),
   mPictureRect(aPictureRect),
   mTextureClient(aClient)
{
}

TextureWrapperImage::~TextureWrapperImage()
{
}

gfx::IntSize
TextureWrapperImage::GetSize() const
{
  return mTextureClient->GetSize();
}

gfx::IntRect
TextureWrapperImage::GetPictureRect() const
{
  return mPictureRect;
}

already_AddRefed<gfx::SourceSurface>
TextureWrapperImage::GetAsSourceSurface()
{
  TextureClientAutoLock autoLock(mTextureClient, OpenMode::OPEN_READ);
  if (!autoLock.Succeeded()) {
    return nullptr;
  }

  RefPtr<DrawTarget> dt = mTextureClient->BorrowDrawTarget();
  if (!dt) {
    return nullptr;
  }

  return dt->Snapshot();
}

TextureClient*
TextureWrapperImage::GetTextureClient(KnowsCompositor* aForwarder)
{
  return mTextureClient;
}

} // namespace layers
} // namespace mozilla
