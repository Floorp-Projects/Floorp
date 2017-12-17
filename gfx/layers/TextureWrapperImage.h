/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_TEXTUREWRAPPINGIMAGE_H_
#define GFX_LAYERS_TEXTUREWRAPPINGIMAGE_H_

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {

// Wraps a TextureClient into an Image. This may only be used on the main
// thread, and only with TextureClients that support BorrowDrawTarget().
class TextureWrapperImage final : public Image
{
public:
  TextureWrapperImage(TextureClient* aClient, const gfx::IntRect& aPictureRect);
  virtual ~TextureWrapperImage();

  gfx::IntSize GetSize() const override;
  gfx::IntRect GetPictureRect() const override;
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override;

private:
  gfx::IntRect mPictureRect;
  RefPtr<TextureClient> mTextureClient;
};

} // namespace layers
} // namespace mozilla

#endif // GFX_LAYERS_TEXTUREWRAPPINGIMAGE_H_
