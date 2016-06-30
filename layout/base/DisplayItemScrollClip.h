/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DISPLAYITEMSCROLLCLIP_H_
#define DISPLAYITEMSCROLLCLIP_H_

#include "mozilla/Assertions.h"
#include "nsString.h"

class nsIScrollableFrame;

namespace mozilla {

class DisplayItemClip;

/**
 * A DisplayItemScrollClip represents a DisplayItemClip from a scrollable
 * frame. This clip is stored on a display item separately from the item's
 * regular DisplayItemClip for async scrolling purposes: The item's regular
 * clip can be fused to the item's contents when drawing layer contents, but
 * scroll clips need to be kept separate so that they can be applied at
 * composition time, after any async scroll offsets have been applied.
 * Scroll clips are created during display list construction when
 * async-scrollable scroll frames are entered. At that point, the current
 * regular clip is cleared on the display list clip state. They are also
 * created for scroll frames that are inactivate and not async scrollable;
 * in that case, mIsAsyncScrollable on the scroll clip will be false.
 * When async-scrollable scroll frames are nested, the inner display items
 * can walk the whole chain of scroll clips via the DisplayItemScrollClip's
 * mParent pointer.
 * Storing scroll clips on display items allows easy access of all scroll
 * frames that affect a certain display item, and it allows some display list
 * operations to compute more accurate clips, for example when computing the
 * bounds of a container item for a frame that contains an async-scrollable
 * scroll frame.
 */
class DisplayItemScrollClip {
public:
  DisplayItemScrollClip(const DisplayItemScrollClip* aParent,
                        nsIScrollableFrame* aScrollableFrame,
                        const DisplayItemClip* aClip,
                        bool aIsAsyncScrollable)
    : mParent(aParent)
    , mScrollableFrame(aScrollableFrame)
    , mClip(aClip)
    , mIsAsyncScrollable(aIsAsyncScrollable)
    , mDepth(aParent ? aParent->mDepth + 1 : 1)
  {
    MOZ_ASSERT(mScrollableFrame);
  }

  /**
   * "Pick descendant" is analogous to the intersection operation on regular
   * clips: In some cases, there are multiple candidate clips that can apply to
   * an item, one of them being the ancestor of the other. This method picks
   * the descendant.
   * Both aClip1 and aClip2 are allowed to be null.
   */
  static const DisplayItemScrollClip*
  PickDescendant(const DisplayItemScrollClip* aClip1,
                const DisplayItemScrollClip* aClip2)
  {
    MOZ_ASSERT(IsAncestor(aClip1, aClip2) || IsAncestor(aClip2, aClip1),
               "one of the scroll clips must be an ancestor of the other");
    return Depth(aClip1) > Depth(aClip2) ? aClip1 : aClip2;
  }

  static const DisplayItemScrollClip*
  PickAncestor(const DisplayItemScrollClip* aClip1,
                const DisplayItemScrollClip* aClip2)
  {
    MOZ_ASSERT(IsAncestor(aClip1, aClip2) || IsAncestor(aClip2, aClip1),
               "one of the scroll clips must be an ancestor of the other");
    return Depth(aClip1) < Depth(aClip2) ? aClip1 : aClip2;
  }


  /**
   * Returns whether aAncestor is an ancestor scroll clip of aDescendant.
   * A scroll clip that's null is considered the root scroll clip.
   */
  static bool IsAncestor(const DisplayItemScrollClip* aAncestor,
                         const DisplayItemScrollClip* aDescendant);

  /**
   * Return a string which contains the list of stringified clips for this
   * scroll clip and its ancestors. aScrollClip can be null.
   */
  static nsCString ToString(const DisplayItemScrollClip* aScrollClip);

  bool HasRoundedCorners() const;

  /**
   * The previous (outer) scroll clip, or null.
   */
  const DisplayItemScrollClip* mParent;

  /**
   * The scrollable frame that this scroll clip is for. Always non-null.
   */
  nsIScrollableFrame* mScrollableFrame;

  /**
   * The clip represented by this scroll clip, relative to mScrollableFrame's
   * reference frame. Can be null.
   */
  const DisplayItemClip* mClip;

  /**
   * Whether this scroll clip is for an async-scrollable scroll frame.
   * Can change during display list construction.
   */
  bool mIsAsyncScrollable;

private:
  static uint32_t Depth(const DisplayItemScrollClip* aSC)
  { return aSC ? aSC->mDepth : 0; }

  const uint32_t mDepth;
};

} // namespace mozilla

#endif /* DISPLAYITEMSCROLLCLIP_H_ */
