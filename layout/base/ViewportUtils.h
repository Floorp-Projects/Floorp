/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ViewportUtils_h
#define mozilla_ViewportUtils_h

#include "Units.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

class nsIFrame;
class nsPresContext;

namespace mozilla {

class PresShell;

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
  template <typename Units = CSSPixel>
  static gfx::Matrix4x4Typed<Units, Units> GetVisualToLayoutTransform(
      layers::ScrollableLayerGuid::ViewID aScrollId);

  /* The functions below apply GetVisualToLayoutTransform() or its inverse
   * to various quantities.
   *
   * To determine the appropriate scroll id to pass into
   * GetVisualToLayoutTransform(), these functions prefer to use the one
   * from InputAPZContext::GetTargetLayerGuid() if one is available.
   * If one is not available, they use the scroll id of the root scroll
   * frame of the pres shell passed in as |aContext| as a fallback.
   */
  static nsPoint VisualToLayout(const nsPoint& aPt, PresShell* aContext);
  static nsRect VisualToLayout(const nsRect& aRect, PresShell* aContext);
  static nsPoint LayoutToVisual(const nsPoint& aPt, PresShell* aContext);

  /*
   * These functions convert the point/rect from layout to visual space
   * by applying the inverse of GetLayoutToVisualTransform() of the root
   * scrollframe of provided presShell.
   */
  static LayoutDevicePoint DocumentRelativeLayoutToVisual(
      const LayoutDevicePoint& aPoint, PresShell* aShell);
  static LayoutDeviceRect DocumentRelativeLayoutToVisual(
      const LayoutDeviceRect& aRect, PresShell* aShell);
  static LayoutDeviceRect DocumentRelativeLayoutToVisual(
      const LayoutDeviceIntRect& aRect, PresShell* aShell);
  static CSSRect DocumentRelativeLayoutToVisual(const CSSRect& aRect,
                                                PresShell* aShell);

  /*
   * These convert aPt/aRect, which is the layout space of aCtx, into a
   * screen-relative visual-space quantity. Formally, we can think of the input
   * as being in RelativeTo{aCtx->PresShell()->GetRootFrame(),
   * ViewportType::Layout} space. And then the function iterates up the chain of
   * presContexts, transforming the point along, until it gets to the
   * RelativeTo{aCtx->GetRootPresContext()->PresShell()->GetRootFrame(),
   * ViewportType::Visual} space. Then it further transforms it into a
   * screen-relative space. Unlike the GetTransformToAncestor family of helper
   * methods, this transformation does NOT include CSS transforms. Also, unlike
   * other ViewportUtils methods, the input is relative to `aCtx` which might be
   * a sub-shell of the presShell with the resolution.
   */
  static LayoutDevicePoint ToScreenRelativeVisual(const LayoutDevicePoint& aPt,
                                                  nsPresContext* aCtx);
  static LayoutDeviceRect ToScreenRelativeVisual(const LayoutDeviceRect& aRect,
                                                 nsPresContext* aCtx);

  /**
   * Returns non-null if |aFrame| is inside the async zoom container but its
   * parent frame is not, thereby making |aFrame| a root of a subtree of
   * frames representing content that is zoomed in. The value returned in such
   * cases is the root scroll frame inside the async zoom container.

   * Callers use this to identify points during frame tree traversal where the
   * visual-to-layout transform needs to be applied.
   */
  static const nsIFrame* IsZoomedContentRoot(const nsIFrame* aFrame);

  /**
   * If |aShell| is in a nested content process, try to infer the resolution
   * at which the enclosing root content
   * document has been painted.
   *
   * Otherwise (if |aShell| is in the chrome or top-level content process),
   * this function returns 1.0.
   *
   * |aShell| must be the root pres shell in its process.
   *
   * This function may not return an accurate answer if there is also
   * a CSS transform enclosing the iframe.
   */
  static Scale2D TryInferEnclosingResolution(PresShell* aShell);
};

// Forward declare explicit instantiations of GetVisualToLayoutTransform() for
// CSSPixel and LayoutDevicePixel, the only two types it gets used with.
// These declarations promise to callers in any translation unit that _some_
// translation unit (in this case, ViewportUtils.cpp) will contain the
// definitions of these instantiations. This allows us to keep the definition
// out-of-line in the source.
extern template CSSToCSSMatrix4x4 ViewportUtils::GetVisualToLayoutTransform<
    CSSPixel>(layers::ScrollableLayerGuid::ViewID);
extern template LayoutDeviceToLayoutDeviceMatrix4x4
    ViewportUtils::GetVisualToLayoutTransform<LayoutDevicePixel>(
        layers::ScrollableLayerGuid::ViewID);

}  // namespace mozilla

#endif /* mozilla_ViewportUtils_h */
