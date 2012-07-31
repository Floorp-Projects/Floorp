/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FRAMEMETRICS_H
#define GFX_FRAMEMETRICS_H

#include "gfxPoint.h"
#include "gfxTypes.h"
#include "nsRect.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace layers {

/**
 * The viewport and displayport metrics for the painted frame at the
 * time of a layer-tree transaction.  These metrics are especially
 * useful for shadow layers, because the metrics values are updated
 * atomically with new pixels.
 */
struct THEBES_API FrameMetrics {
public:
  // We use IDs to identify frames across processes.
  typedef PRUint64 ViewID;
  static const ViewID NULL_SCROLL_ID;   // This container layer does not scroll.
  static const ViewID ROOT_SCROLL_ID;   // This is the root scroll frame.
  static const ViewID START_SCROLL_ID;  // This is the ID that scrolling subframes
                                        // will begin at.

  FrameMetrics()
    : mViewport(0, 0, 0, 0)
    , mContentRect(0, 0, 0, 0)
    , mViewportScrollOffset(0, 0)
    , mScrollId(NULL_SCROLL_ID)
    , mCSSContentRect(0, 0, 0, 0)
    , mResolution(1, 1)
  {}

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const
  {
    return (mViewport.IsEqualEdges(aOther.mViewport) &&
            mViewportScrollOffset == aOther.mViewportScrollOffset &&
            mDisplayPort.IsEqualEdges(aOther.mDisplayPort) &&
            mScrollId == aOther.mScrollId);
  }
  bool operator!=(const FrameMetrics& aOther) const
  {
    return !operator==(aOther);
  }

  bool IsDefault() const
  {
    return (FrameMetrics() == *this);
  }

  bool IsRootScrollable() const
  {
    return mScrollId == ROOT_SCROLL_ID;
  }

  bool IsScrollable() const
  {
    return mScrollId != NULL_SCROLL_ID;
  }

  // These are all in layer coordinate space.
  nsIntRect mViewport;
  nsIntRect mContentRect;
  nsIntPoint mViewportScrollOffset;
  nsIntRect mDisplayPort;
  ViewID mScrollId;

  // Consumers often want to know the origin/size before scaling to pixels
  // so we record this as well.
  gfx::Rect mCSSContentRect;

  // This represents the resolution at which the associated layer
  // will been rendered.
  gfxSize mResolution;
};

}
}

#endif /* GFX_FRAMEMETRICS_H */
