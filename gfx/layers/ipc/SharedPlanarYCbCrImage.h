/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
  ~SharedPlanarYCbCrImage();

public:
  virtual TextureClient* GetTextureClient(TextureForwarder* aForwarder) override;
  virtual uint8_t* GetBuffer() override;

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  virtual bool CopyData(const PlanarYCbCrData& aData) override;
  virtual bool AdoptData(const Data &aData) override;

  virtual bool Allocate(PlanarYCbCrData& aData);
  virtual uint8_t* AllocateAndGetNewBuffer(uint32_t aSize) override;

  virtual bool IsValid() override;

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  RefPtr<TextureClient> mTextureClient;
  RefPtr<ImageClient> mCompositable;
};

} // namespace layers
} // namespace mozilla

#endif
