/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ViewportUtils_h
#define mozilla_ViewportUtils_h

#include "Units.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

namespace mozilla {

class ViewportUtils {
 public:
  /* Return a transform to be applied to the coordinates of input events
     targeting content inside the scroll frame identified by |aScrollId|, which
     converts from "visual coordinates" (which are the coordinates events have
     when they arrive from APZ) to "layout coordinates" (which are the
     coordinates used in most places by layout code). The transform has two
     components:
     1. The pres shell resolution, representing the pinch-zoom scale
        (if the scroll frame |aScrollId| is inside the resolution, which
        is most of the time).
     2. A translation representing async scrolling. This can contain:
         - For any scroll frame, a scroll component resulting from the main
           thread incompletely applying an APZ-requested scroll position.
         - For the RCD-RSF only, a persistent component representing the
           offset of the visual viewport relative to the layout viewport.
       The translation is accumulated for all scroll frames form |aScrollId|
       up to the root, using values populated in
       APZCCallbackHelper::UpdateCallbackTransform. See that method's
       documentation for additional details. */
  static CSSToCSSMatrix4x4 GetVisualToLayoutTransform(
      layers::ScrollableLayerGuid::ViewID aScrollId);
};

}  // namespace mozilla

#endif /* mozilla_ViewportUtils_h */
