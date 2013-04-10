/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_TEXTURECHILD_H
#define MOZILLA_LAYERS_TEXTURECHILD_H

#include "mozilla/layers/PTextureChild.h"
#include "CompositableClient.h"

namespace mozilla {
namespace layers {

class TextureClient;

class TextureChild : public PTextureChild
{
public:
  TextureChild()
  : mTextureClient(nullptr)
  {
    MOZ_COUNT_CTOR(TextureClient);
  }
  ~TextureChild()
  {
    MOZ_COUNT_DTOR(TextureClient);
  }

  void SetClient(TextureClient* aTextureClient)
  {
    mTextureClient = aTextureClient;
  }

  CompositableClient* GetCompositableClient();
  TextureClient* GetTextureClient() const
  {
    return mTextureClient;
  }

  void Destroy();

private:
  TextureClient* mTextureClient;
};

} // namespace
} // namespace

#endif
