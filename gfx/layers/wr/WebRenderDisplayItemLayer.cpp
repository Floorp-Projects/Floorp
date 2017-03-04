/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderDisplayItemLayer.h"

#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "nsDisplayList.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace layers {

void
WebRenderDisplayItemLayer::RenderLayer()
{
  if (mItem) {
    // We might have recycled this layer. Throw away the old commands.
    mCommands.Clear();
    mParentCommands.Clear();
    mItem->CreateWebRenderCommands(mCommands, mParentCommands, this);
  }
  // else we have an empty transaction and just use the
  // old commands.

  WrBridge()->AddWebRenderCommands(mCommands);
  WrBridge()->AddWebRenderParentCommands(mParentCommands);
}

uint64_t
WebRenderDisplayItemLayer::SendImageContainer(ImageContainer* aContainer)
{
  if (mImageContainer != aContainer) {
    AutoLockImage autoLock(aContainer);
    Image* image = autoLock.GetImage();
    if (!image) {
      return 0;
    }

    if (!mImageClient) {
      mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                    WrBridge(),
                                                    TextureFlags::DEFAULT);
      if (!mImageClient) {
        return 0;
      }
      mImageClient->Connect();
    }

    if (!mExternalImageId) {
      MOZ_ASSERT(mImageClient);
      mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mImageClient);
    }
    MOZ_ASSERT(mExternalImageId);

    if (mImageClient && !mImageClient->UpdateImage(aContainer, /* unused */0)) {
      return 0;
    }
    mImageContainer = aContainer;
  }

  return mExternalImageId;
}

} // namespace layers
} // namespace mozilla
