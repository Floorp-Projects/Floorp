/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H

#include <queue>

#include "mozilla/layers/TextureHost.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {

namespace wr {
class WebRenderAPI;
}

namespace layers {

class CompositableHost;
class WebRenderTextureHost;

class WebRenderCompositableHolder final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderCompositableHolder)

  explicit WebRenderCompositableHolder();

protected:
  ~WebRenderCompositableHolder();

public:
  void Destroy();
  void HoldExternalImage(const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture);
  void Update(const wr::Epoch& aEpoch);

private:

  struct ForwardingTextureHosts {
    ForwardingTextureHosts(const wr::Epoch& aEpoch, TextureHost* aTexture)
      : mEpoch(aEpoch)
      , mTexture(aTexture)
    {}
    wr::Epoch mEpoch;
    CompositableTextureHostRef mTexture;
  };

  // Holds forwarding WebRenderTextureHosts.
  std::queue<ForwardingTextureHosts> mWebRenderTextureHosts;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H */
