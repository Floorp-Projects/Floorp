/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERTEXTUREHOSTWRAPPER_H
#define MOZILLA_GFX_WEBRENDERTEXTUREHOSTWRAPPER_H

#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {

namespace wr {
class TransactionBuilder;
}

namespace layers {

class WebRenderTextureHost;
class AsyncImagePipelineManager;

class WebRenderTextureHostWrapper {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderTextureHostWrapper)

 public:
  explicit WebRenderTextureHostWrapper(AsyncImagePipelineManager* aManager);

  void UpdateWebRenderTextureHost(wr::TransactionBuilder& aTxn,
                                  WebRenderTextureHost* aTextureHost);

  wr::ExternalImageId GetExternalImageKey() { return mExternalImageId; }

 protected:
  virtual ~WebRenderTextureHostWrapper();

  RefPtr<WebRenderTextureHost> mWrTextureHost;
  wr::ExternalImageId mExternalImageId;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_WEBRENDERTEXTUREHOSTWRAPPER_H
