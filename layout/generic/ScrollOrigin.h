/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollOrigin_h_
#define mozilla_ScrollOrigin_h_

namespace mozilla {

// A scroll origin is a bit of a combination of "what part of the code caused
// this scroll" and "what special properties does this scroll have, in the
// context of what caused it". See specific comments below.
enum class ScrollOrigin : uint8_t {
  // This is used as an initial value for the "LastScrollOrigin" property on
  // scrollable frames. It is not intended to be an actual scroll origin, but
  // a sentinel value that indicates that there was no "last scroll". It is
  // used similarly for the "LastSmoothScrollOrigin" property, to indicate
  // no smooth scroll is in progress.
  None,

  // This is a default value that we use when we don't know of a more specific
  // value that we can use.
  NotSpecified,
  // The scroll came from APZ code.
  Apz,
  // The scroll came from an attempt at restoring a scroll position saved in
  // the bfcache or history.
  Restore,
  // The scroll came from a "relative" scroll method like ScrollBy, where the
  // scroll destination is indicated by a delta from the current position
  // instead of an absolute value.
  Relative,
  // The scroll came from an attempt by the main thread to re-clamp the scroll
  // position after a reflow.
  Clamp,
  // The scroll came from a scroll adjustment triggered by scroll anchoring.
  AnchorAdjustment,

  // The following scroll origins also are associated with prefs of the form
  //   general.smoothScroll.<origin>(.*)
  // e.g. general.smoothScroll.lines indicates whether or not a scroll with
  // origin Lines will be animated smoothly, and e.g. subprefs like
  // general.smoothScroll.lines.durationMinMS control some of the animation
  // timing behavior.

  // The scroll came from some sort of input that's not one of the above or
  // below values. Generally this means it came from a content-exposed API,
  // like window.scrollTo, but may also be from other sources that don't need
  // any particular special handling.
  Other,
  // The scroll was originated by pixel-scrolling input device (e.g. precision
  // mouse wheel).
  Pixels,
  // The scroll was originated by a line-scrolling input device (e.g. up/down
  // keyboard buttons).
  Lines,
  // The scroll was originated by a page-scrolling input device (e.g. pgup/
  // pgdn keyboard buttons).
  Pages,
  // The scroll was originated by a mousewheel that scrolls by lines.
  MouseWheel,
  // The scroll was originated by moving the scrollbars.
  Scrollbars,
};

}  // namespace mozilla

#endif  // mozilla_ScrollOrigin_h_
