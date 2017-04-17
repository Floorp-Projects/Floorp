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
WebRenderDisplayItemLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }

  Maybe<WrImageMask> mask = BuildWrMaskLayer(false);
  WrImageMask* imageMask = mask.ptrOr(nullptr);
  if (imageMask) {
    gfx::Rect rect = TransformedVisibleBoundsRelativeToParent();
    gfx::Rect overflow(0.0, 0.0, rect.width, rect.height);
    aBuilder.PushScrollLayer(wr::ToWrRect(rect),
                             wr::ToWrRect(overflow),
                             imageMask);
  }

  if (mItem) {
    wr::DisplayListBuilder builder(WrBridge()->GetPipeline());
    // We might have recycled this layer. Throw away the old commands.
    mParentCommands.Clear();
    mItem->CreateWebRenderCommands(builder, mParentCommands, this);
    mBuiltDisplayList = builder.Finalize();
  }
  // else we have an empty transaction and just use the
  // old commands.

  aBuilder.PushBuiltDisplayList(Move(mBuiltDisplayList));
  WrBridge()->AddWebRenderParentCommands(mParentCommands);

  if (imageMask) {
    aBuilder.PopScrollLayer();
  }
}

Maybe<wr::ImageKey>
WebRenderDisplayItemLayer::SendImageContainer(ImageContainer* aContainer,
                                              nsTArray<layers::WebRenderParentCommand>& aParentCommands)
{
  if (mImageContainer != aContainer) {
    AutoLockImage autoLock(aContainer);
    Image* image = autoLock.GetImage();
    if (!image) {
      return Nothing();
    }

    if (!mImageClient) {
      mImageClient = ImageClient::CreateImageClient(CompositableType::IMAGE,
                                                    WrBridge(),
                                                    TextureFlags::DEFAULT);
      if (!mImageClient) {
        return Nothing();
      }
      mImageClient->Connect();
    }

    if (!mExternalImageId) {
      MOZ_ASSERT(mImageClient);
      mExternalImageId = WrBridge()->AllocExternalImageIdForCompositable(mImageClient);
    }
    MOZ_ASSERT(mExternalImageId);

    if (!mImageClient->UpdateImage(aContainer, /* unused */0)) {
      return Nothing();
    }
    mImageContainer = aContainer;
  }

  WrImageKey key;
  key.mNamespace = WrBridge()->GetNamespace();
  key.mHandle = WrBridge()->GetNextResourceId();
  aParentCommands.AppendElement(layers::OpAddExternalImage(
                                mExternalImageId,
                                key));
  WrManager()->AddImageKeyForDiscard(key);
  return Some(key);
}

} // namespace layers
} // namespace mozilla
