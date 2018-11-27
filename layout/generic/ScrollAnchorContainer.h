/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollAnchorContainer_h_
#define mozilla_layout_ScrollAnchorContainer_h_

namespace mozilla {
class ScrollFrameHelper;
}  // namespace mozilla

namespace mozilla {
namespace layout {

/**
 * A scroll anchor container finds a descendent element of a scrollable frame
 * to be an anchor node. After every reflow, the scroll anchor will apply
 * scroll adjustments to keep the anchor node in the same relative position.
 *
 * See: https://drafts.csswg.org/css-scroll-anchoring/
 */
class ScrollAnchorContainer final {
 public:
  explicit ScrollAnchorContainer(ScrollFrameHelper* aScrollFrame);
  ~ScrollAnchorContainer();

  /**
   * Returns the nearest scroll anchor container that could select aFrame as an
   * anchor node.
   */
  static ScrollAnchorContainer* FindFor(nsIFrame* aFrame);

  /**
   * Returns the frame that owns this scroll anchor container. This is always
   * non-null.
   */
  nsIFrame* Frame() const;

  /**
   * Returns the frame that owns this scroll anchor container as a scrollable
   * frame. This is always non-null.
   */
  nsIScrollableFrame* ScrollableFrame() const;

 private:
  // The owner of this scroll anchor container
  ScrollFrameHelper* mScrollFrame;
};

}  // namespace layout
}  // namespace mozilla

#endif  // mozilla_layout_ScrollAnchorContainer_h_
