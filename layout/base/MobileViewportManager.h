/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MobileViewportManager_h_
#define MobileViewportManager_h_

#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/MVMContext.h"
#include "mozilla/PresShellForwards.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "Units.h"

class nsViewportInfo;

namespace mozilla {
class MVMContext;
namespace dom {
class Document;
class EventTarget;
}  // namespace dom
}  // namespace mozilla

class MobileViewportManager final : public nsIDOMEventListener,
                                    public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  /* The MobileViewportManager might be required to handle meta-viewport tags
   * and changes, or it might not (e.g. if we are in a desktop-zooming setup).
   * This enum indicates which mode the manager is in. It might make sense to
   * split these two "modes" into two separate classes but for now they have a
   * bunch of shared code and it's uncertain if that shared set will expand or
   * contract. */
  enum class ManagerType { VisualAndMetaViewport, VisualViewportOnly };

  explicit MobileViewportManager(mozilla::MVMContext* aContext,
                                 ManagerType aType);
  void Destroy();

  ManagerType GetManagerType() { return mManagerType; }

  /* Provide a resolution to use during the first paint instead of the default
   * resolution computed from the viewport info metadata. This is in the same
   * "units" as the argument to nsDOMWindowUtils::SetResolutionAndScaleTo.
   * Also takes the previous display dimensions as they were at the time the
   * resolution was stored in order to correctly adjust the resolution if the
   * device was rotated in the meantime. */
  void SetRestoreResolution(float aResolution,
                            mozilla::LayoutDeviceIntSize aDisplaySize);

  /* Compute the "intrinsic resolution", which is the smallest resolution at
   * which the layout viewport fills the visual viewport. (In typical
   * scenarios, where the aspect ratios of the two viewports match, it's the
   * resolution at which they are the same size.)
   *
   * The returned resolution is suitable for passing to
   * PresShell::SetResolutionAndScaleTo(). It's not in typed units for
   * reasons explained at the declaration of FrameMetrics::mPresShellResolution.
   */
  float ComputeIntrinsicResolution() const;

  /* The only direct calls to this should be in test code.
   * Normally, it gets called by HandleEvent().
   */
  void HandleDOMMetaAdded();

 private:
  void SetRestoreResolution(float aResolution);

 public:
  /* Notify the MobileViewportManager that a reflow is about to happen. This
   * triggers the MVM to update its internal notion of display size and CSS
   * viewport, so that code that queries those during the reflow gets an
   * up-to-date value.
   */
  void UpdateSizesBeforeReflow();

  /* Notify the MobileViewportManager that a reflow was requested in the
   * presShell.*/
  void RequestReflow(bool aForceAdjustResolution);

  /* Notify the MobileViewportManager that the resolution on the presShell was
   * updated, and the visual viewport size needs to be updated. */
  void ResolutionUpdated(mozilla::ResolutionChangeOrigin aOrigin);

  /* Called to compute the initial viewport on page load or before-first-paint,
   * whichever happens first. Also called directly if we are created after the
   * presShell is initialized. */
  void SetInitialViewport();

  const mozilla::LayoutDeviceIntSize& DisplaySize() const {
    return mDisplaySize;
  };

  /*
   * Shrink the content to fit it to the display width if no initial-scale is
   * specified and if the content is still wider than the display width.
   */
  void ShrinkToDisplaySizeIfNeeded();

  /*
   * Similar to UpdateVisualViewportSize but this should be called only when we
   * need to update visual viewport size in response to dynamic toolbar
   * transitions.
   * This function doesn't cause any reflows, just fires a visual viewport
   * resize event.
   */
  void UpdateVisualViewportSizeByDynamicToolbar(
      mozilla::ScreenIntCoord aToolbarHeight);

  nsSize GetVisualViewportSizeUpdatedByDynamicToolbar() const {
    return mVisualViewportSizeUpdatedByDynamicToolbar;
  }

  /*
   * This refreshes the visual viewport size based on the most recently
   * available information. It is intended to be called in particular after
   * the root scrollframe does a reflow, which may make scrollbars appear or
   * disappear if the content changed size.
   */
  void UpdateVisualViewportSizeForPotentialScrollbarChange();

  /*
   * Returns the composition size in CSS units when zoomed to the intrinsic
   * scale.
   */
  mozilla::CSSSize GetIntrinsicCompositionSize() const;

  mozilla::ParentLayerSize GetCompositionSizeWithoutDynamicToolbar() const;

  static mozilla::LazyLogModule gLog;

 private:
  ~MobileViewportManager();

  /* Main helper method to update the CSS viewport and any other properties that
   * need updating. */
  void RefreshViewportSize(bool aForceAdjustResolution);

  /* Secondary main helper method to update just the visual viewport size. */
  void RefreshVisualViewportSize();

  /* Helper to clamp the given zoom by the min/max in the viewport info. */
  mozilla::CSSToScreenScale ClampZoom(
      const mozilla::CSSToScreenScale& aZoom,
      const nsViewportInfo& aViewportInfo) const;

  /* Helper to update the given zoom according to changed display and viewport
   * widths. */
  mozilla::CSSToScreenScale ScaleZoomWithDisplayWidth(
      const mozilla::CSSToScreenScale& aZoom,
      const float& aDisplayWidthChangeRatio,
      const mozilla::CSSSize& aNewViewport,
      const mozilla::CSSSize& aOldViewport);

  mozilla::CSSToScreenScale ResolutionToZoom(
      const mozilla::LayoutDeviceToLayerScale& aResolution) const;
  mozilla::LayoutDeviceToLayerScale ZoomToResolution(
      const mozilla::CSSToScreenScale& aZoom) const;

  /* Updates the presShell resolution and the visual viewport size for various
   * types of changes. */
  void UpdateResolutionForFirstPaint(const mozilla::CSSSize& aViewportSize);
  void UpdateResolutionForViewportSizeChange(
      const mozilla::CSSSize& aViewportSize,
      const mozilla::Maybe<float>& aDisplayWidthChangeRatio);
  void UpdateResolutionForContentSizeChange(
      const mozilla::CSSSize& aContentSize);

  void ApplyNewZoom(const mozilla::ScreenIntSize& aDisplaySize,
                    const mozilla::CSSToScreenScale& aNewZoom);

  void UpdateVisualViewportSize(const mozilla::ScreenIntSize& aDisplaySize,
                                const mozilla::CSSToScreenScale& aZoom);

  /* Updates the displayport margins for the presShell's root scrollable frame
   */
  void UpdateDisplayPortMargins();

  /* Helper function for ComputeIntrinsicResolution(). */
  mozilla::CSSToScreenScale ComputeIntrinsicScale(
      const nsViewportInfo& aViewportInfo,
      const mozilla::ScreenIntSize& aDisplaySize,
      const mozilla::CSSSize& aViewportOrContentSize) const;

  /*
   * Returns the screen size subtracted the scrollbar sizes from |aDisplaySize|.
   */
  mozilla::ScreenIntSize GetCompositionSize(
      const mozilla::ScreenIntSize& aDisplaySize) const;

  mozilla::CSSToScreenScale GetZoom() const;

  RefPtr<mozilla::MVMContext> mContext;
  ManagerType mManagerType;
  bool mIsFirstPaint;
  bool mPainted;
  mozilla::LayoutDeviceIntSize mDisplaySize;
  mozilla::CSSSize mMobileViewportSize;
  mozilla::Maybe<float> mRestoreResolution;
  mozilla::Maybe<mozilla::ScreenIntSize> mRestoreDisplaySize;
  /*
   * The visual viewport size updated by the dynamic toolbar transitions. This
   * is typically used for the VisualViewport width/height APIs.
   * NOTE: If you want to use this value, you should make sure to flush
   * position:fixed elements layout and update
   * FrameMetrics.mFixedLayerMargins to conform with this value.
   */
  nsSize mVisualViewportSizeUpdatedByDynamicToolbar;
};

#endif
