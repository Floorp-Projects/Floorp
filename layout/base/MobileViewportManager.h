/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MobileViewportManager_h_
#define MobileViewportManager_h_

#include "mozilla/Maybe.h"
#include "mozilla/MVMContext.h"
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

  explicit MobileViewportManager(mozilla::MVMContext* aContext);
  void Destroy();

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

 private:
  void SetRestoreResolution(float aResolution);

 public:
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
  void ShrinkToDisplaySizeIfNeeded(nsViewportInfo& aViewportInfo,
                                   const mozilla::ScreenIntSize& aDisplaySize);

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

  /* Helper enum for UpdateResolution().
   * UpdateResolution() is called twice during RefreshViewportSize():
   * First, to choose an initial resolution based on the viewport size.
   * Second, after reflow when we know the content size, to make any
   * necessary adjustments to the resolution.
   * This enumeration discriminates between the two situations.
   */
  enum class UpdateType { ViewportSize, ContentSize };

  /* Updates the presShell resolution and the visual viewport size. */
  void UpdateResolution(const nsViewportInfo& aViewportInfo,
                        const mozilla::ScreenIntSize& aDisplaySize,
                        const mozilla::CSSSize& aViewportOrContentSize,
                        const mozilla::Maybe<float>& aDisplayWidthChangeRatio,
                        UpdateType aType);

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

  RefPtr<mozilla::MVMContext> mContext;
  bool mIsFirstPaint;
  bool mPainted;
  mozilla::LayoutDeviceIntSize mDisplaySize;
  mozilla::CSSSize mMobileViewportSize;
  mozilla::Maybe<float> mRestoreResolution;
  mozilla::Maybe<mozilla::ScreenIntSize> mRestoreDisplaySize;
};

#endif
