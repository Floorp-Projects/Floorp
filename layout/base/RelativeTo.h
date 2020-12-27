/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RelativeTo_h
#define mozilla_RelativeTo_h

#include <ostream>

class nsIFrame;

namespace mozilla {

// A flag that can be used to annotate a frame to distinguish coordinates
// relative to the viewport frame as being in layout or visual coordinates.
enum class ViewportType { Layout, Visual };

// A struct that combines a frame with a ViewportType annotation. The
// combination completely describes what a set of coordinates is "relative to".
// Notes on expected usage:
//  - The boundary between visual and layout coordinates is approximately
//    at the root content document (RCD)'s ViewportFrame, which we'll
//    call "RCD-VF".
//  - Coordinates relative to the RCD-VF's descendants (other than the
//    RCD's viewport scrollbar frames) should be in layout coordinates.
//  - Coordinates relative to the RCD-VF's ancestors should be in visual
//    coordinates (note that in an e10s setup, the RCD-VF doesn't
//    typically have in-process ancestors).
//  - Coordinates relative to the RCD-VF itself can be in either layout
//    or visual coordinates.
struct RelativeTo {
  const nsIFrame* mFrame = nullptr;
  // Choose ViewportType::Layout as the default as this is what the vast
  // majority of layout code deals with.
  ViewportType mViewportType = ViewportType::Layout;
  bool operator==(const RelativeTo& aOther) const {
    return mFrame == aOther.mFrame && mViewportType == aOther.mViewportType;
  }
  friend std::ostream& operator<<(std::ostream& aOs, const RelativeTo& aR) {
    return aOs << "{" << aR.mFrame << ", "
               << (aR.mViewportType == ViewportType::Visual ? "visual"
                                                            : "layout")
               << "}";
  }
};

}  // namespace mozilla

#endif  // mozilla_RelativeTo_h