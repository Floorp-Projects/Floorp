/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_RENDERROOTBOUNDARY_H
#define GFX_RENDERROOTBOUNDARY_H

#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

// clang-format off
// A display list can have multiple nsDisplayRenderRoot instances in it, one
// at each point that the display list transitions from being in one render root
// to another. In particular, this means that a display list can transition
// from e.g. the "Default" render root to the "Content" render root multiple
// times. When we process this display list in WebRenderCommandBuilder, we build
// a WebRenderScrollData for each render root, and they are "connected" by the
// referent render root property on the WebRenderLayerScrollData instances.
// (This is similar to how the layers id referent property works, i.e. a WRLSD
// item with a referent is an attachment point for a subtree from a different
// render root.) But given that there are multiple transitions to the same
// "child" render root, we need an additional data point to uniquely identify
// each of the transitions, so that we can attach the correct scroll data
// subtree at each transition. Failure to uniquely identify the transitions
// means that the entire "child" render root's scroll data tree will get
// attached at each transition. The RenderRootBoundary class serves this
// purpose.
//
// Example time! Consider the following display list structure:
//
//           R        // root of the display list
//          / \       //
//         A   B      // items in Default render root that generate scroll data
//        /   / \     //
//       C   D   E    // nsDisplayRenderRoot items transitioning to content
//      / \     /     //
//     F   G   H      // items in Content render root that generate scroll data
//
// In this example, the Default render root WebRenderScrollData will contain
// 6 WRLSD items, one for each of R, A, B, C, D, E. Of these, C, D, and E will
// have mReferentRenderRoot properties set. The WebRenderScrollData for the
// Content render root will end up looking like this:
//
//          Dummy root         //
//          /        \         //
//        C-Root    E-Root     //
//       /    |       |        //
//      F     G       H        //
//
// The RenderRootBoundary item on C will point to C-Root, and likewise for E.
// The RenderRootBoundary item on D will be valid, but there will be no
// corresponding subtree in the Content-side WebRenderScrollData. C-Root and
// E-Root are created via WebRenderScrollDataCollection::AppendWrapper, and
// have their mBoundaryRoot property set to the same RenderRootBoundary value
// as the mReferentRenderRoot properties from C and E respectively.
// clang-format on
class RenderRootBoundary {
 public:
  explicit RenderRootBoundary(wr::RenderRoot aChildType)
      : mChildType(aChildType) {
    static uint64_t sCounter = 0;
    mId = ++sCounter;
  }

  wr::RenderRoot GetChildType() const { return mChildType; }

  bool operator==(const RenderRootBoundary& aOther) const {
    return mChildType == aOther.mChildType && mId == aOther.mId;
  }

  friend struct IPC::ParamTraits<RenderRootBoundary>;
  // constructor for IPC
  RenderRootBoundary() = default;

 private:
  wr::RenderRoot mChildType;
  // The id is what distinguishes different transition points within a display
  // list (i.e. what would be different in C, D, and E in the example above).
  uint64_t mId;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_RENDERROOTBOUNDARY_H
