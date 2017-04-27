/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H

#include <queue>

#include "mozilla/layers/TextureHost.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsClassHashtable.h"

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
  void AddPipeline(const wr::PipelineId& aPipelineId);
  void RemovePipeline(const wr::PipelineId& aPipelineId);
  void HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture);
  void Update(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch);

  TimeStamp GetCompositionTime() const {
    return mCompositionTime;
  }
  void SetCompositionTime(TimeStamp aTimeStamp) {
    mCompositionTime = aTimeStamp;
    if (!mCompositionTime.IsNull() && !mCompositeUntilTime.IsNull() &&
        mCompositionTime >= mCompositeUntilTime) {
      mCompositeUntilTime = TimeStamp();
    }
  }
  void CompositeUntil(TimeStamp aTimeStamp) {
    if (mCompositeUntilTime.IsNull() ||
        mCompositeUntilTime < aTimeStamp) {
      mCompositeUntilTime = aTimeStamp;
    }
  }
  TimeStamp GetCompositeUntilTime() const {
    return mCompositeUntilTime;
  }

private:

  struct ForwardingTextureHost {
    ForwardingTextureHost(const wr::Epoch& aEpoch, TextureHost* aTexture)
      : mEpoch(aEpoch)
      , mTexture(aTexture)
    {}
    wr::Epoch mEpoch;
    CompositableTextureHostRef mTexture;
  };

  struct PipelineTexturesHolder {
    // Holds forwarding WebRenderTextureHosts.
    std::queue<ForwardingTextureHost> mTextureHosts;
  };

  nsClassHashtable<nsUint64HashKey, PipelineTexturesHolder> mPipelineTexturesHolders;

  // Render time for the current composition.
  TimeStamp mCompositionTime;

  // When nonnull, during rendering, some compositable indicated that it will
  // change its rendering at this time. In order not to miss it, we composite
  // on every vsync until this time occurs (this is the latest such time).
  TimeStamp mCompositeUntilTime;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H */
