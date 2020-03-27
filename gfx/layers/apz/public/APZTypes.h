/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZTypes_h
#define mozilla_layers_APZTypes_h

#include "LayersTypes.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {

namespace layers {

// This a simple structure that wraps a ScrollableLayerGuid and a RenderRoot.
// It is needed on codepaths shared with WebRender, where we need to propagate
// the RenderRoot information along with the ScrollableLayerGuid (as each
// scrollable frame belongs to exactly one RenderRoot, and APZ needs to record
// that information on the APZC instances).
struct SLGuidAndRenderRoot {
  ScrollableLayerGuid mScrollableLayerGuid;
  wr::RenderRoot mRenderRoot;

  // needed for IPDL, but shouldn't be used otherwise!
  SLGuidAndRenderRoot() : mRenderRoot(wr::RenderRoot::Default) {}

  SLGuidAndRenderRoot(LayersId aLayersId, uint32_t aPresShellId,
                      ScrollableLayerGuid::ViewID aScrollId,
                      wr::RenderRoot aRenderRoot)
      : mScrollableLayerGuid(aLayersId, aPresShellId, aScrollId),
        mRenderRoot(aRenderRoot) {}

  SLGuidAndRenderRoot(const ScrollableLayerGuid& other,
                      wr::RenderRoot aRenderRoot)
      : mScrollableLayerGuid(other), mRenderRoot(aRenderRoot) {}
};

template <int LogLevel>
gfx::Log<LogLevel>& operator<<(gfx::Log<LogLevel>& log,
                               const SLGuidAndRenderRoot& aGuid) {
  return log << '(' << aGuid.mScrollableLayerGuid << ','
             << (int)aGuid.mRenderRoot << ')';
}

}  // namespace layers

}  // namespace mozilla

#endif /* mozilla_layers_APZTypes_h */
