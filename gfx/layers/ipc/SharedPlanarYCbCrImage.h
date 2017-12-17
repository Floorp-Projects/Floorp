/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>                     // for uint8_t, uint32_t
#include "ImageContainer.h"             // for PlanarYCbCrImage, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR

#ifndef MOZILLA_LAYERS_SHAREDPLANARYCBCRIMAGE_H
#define MOZILLA_LAYERS_SHAREDPLANARYCBCRIMAGE_H

namespace mozilla {
namespace layers {

class ImageClient;
class TextureClient;

class SharedPlanarYCbCrImage : public PlanarYCbCrImage
{
public:
  explicit SharedPlanarYCbCrImage(ImageClient* aCompositable);

protected:
  virtual ~SharedPlanarYCbCrImage();

public:
  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override;
  uint8_t* GetBuffer() const override;

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  bool CopyData(const PlanarYCbCrData& aData) override;
  bool AdoptData(const Data& aData) override;

  bool Allocate(PlanarYCbCrData& aData);

  bool IsValid() const override;

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  RefPtr<TextureClient> mTextureClient;
  RefPtr<ImageClient> mCompositable;
};

} // namespace layers
} // namespace mozilla

#endif
