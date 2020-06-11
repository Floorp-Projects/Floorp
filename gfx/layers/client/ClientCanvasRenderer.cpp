/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientCanvasRenderer.h"

#include "ClientCanvasLayer.h"

namespace mozilla {
namespace layers {

CompositableForwarder* ClientCanvasRenderer::GetForwarder() {
  return mLayer->Manager()->AsShadowForwarder();
}

bool ClientCanvasRenderer::CreateCompositable() {
  if (!mCanvasClient) {
    TextureFlags flags = TextureFlags::DEFAULT;
    if (mOriginPos == gl::OriginPos::BottomLeft) {
      flags |= TextureFlags::ORIGIN_BOTTOM_LEFT;
    }

    if (IsOpaque()) {
      flags |= TextureFlags::IS_OPAQUE;
    }

    if (!mIsAlphaPremultiplied) {
      flags |= TextureFlags::NON_PREMULTIPLIED;
    }

    mCanvasClient = CanvasClient::CreateCanvasClient(GetCanvasClientType(),
                                                     GetForwarder(), flags);
    if (!mCanvasClient) {
      return false;
    }

    if (mLayer->HasShadow()) {
      if (mAsyncRenderer) {
        static_cast<CanvasClientBridge*>(mCanvasClient.get())->SetLayer(mLayer);
      } else if (mOOPRenderer) {
        static_cast<CanvasClientOOP*>(mCanvasClient.get())
            ->SetLayer(mLayer, mOOPRenderer);
      } else {
        mCanvasClient->Connect();
        GetForwarder()->AsLayerForwarder()->Attach(mCanvasClient, mLayer);
      }
    }
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla
