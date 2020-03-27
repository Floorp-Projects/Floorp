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

// This struct provides a way to identify a specific "render root" within a
// specific layer subtree. With WebRender enabled, parts of a layer subtree
// may get split into different WebRender "documents" (aka "render roots"),
// and APZ often needs to know which "render root" a particular message is
// targeted at, so that it can use the correct WebRender document id when
// interfacing with WebRender. For codepaths that are never used with
// WebRender, the static WrRootId::NonWebRender constructor can be used to
// obtain an instance of this struct.
struct WRRootId {
  LayersId mLayersId;
  wr::RenderRoot mRenderRoot;

  WRRootId() = default;

  static WRRootId NonWebRender(LayersId aLayersId) {
    return WRRootId(aLayersId, wr::RenderRoot::Default);
  }

  WRRootId(LayersId aLayersId, wr::RenderRoot aRenderRoot)
      : mLayersId(aLayersId), mRenderRoot(aRenderRoot) {}

  WRRootId(wr::PipelineId aLayersId, wr::DocumentId aRenderRootId)
      : mLayersId(AsLayersId(aLayersId)),
        mRenderRoot(RenderRootFromId(aRenderRootId)) {}

  bool operator==(const WRRootId& aOther) const {
    return mRenderRoot == aOther.mRenderRoot && mLayersId == aOther.mLayersId;
  }

  bool operator!=(const WRRootId& aOther) const { return !(*this == aOther); }

  bool IsValid() const { return mLayersId.IsValid(); }

  // Helper struct that allow this class to be used as a key in
  // std::unordered_map like so:
  //   std::unordered_map<WRRootId, ValueType, WRRootId::HashFn> myMap;
  struct HashFn {
    std::size_t operator()(const WRRootId& aKey) const {
      return HashGeneric((uint64_t)aKey.mLayersId, (uint8_t)aKey.mRenderRoot);
    }
  };
};

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

  WRRootId GetWRRootId() const {
    return WRRootId(mScrollableLayerGuid.mLayersId, mRenderRoot);
  }
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
