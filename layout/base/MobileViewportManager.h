/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MobileViewportManager_h_
#define MobileViewportManager_h_

#include "mozilla/Maybe.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "Units.h"

class nsIDOMEventTarget;
class nsIDocument;
class nsIPresShell;
class nsViewportInfo;

class MobileViewportManager final : public nsIDOMEventListener
                                  , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  MobileViewportManager(nsIPresShell* aPresShell,
                        nsIDocument* aDocument);
  void Destroy();

  /* Provide a resolution to use during the first paint instead of the default
   * resolution computed from the viewport info metadata. This is in the same
   * "units" as the argument to nsDOMWindowUtils::SetResolutionAndScaleTo.
   * Also takes the previous display dimensions as they were at the time the
   * resolution was stored in order to correctly adjust the resolution if the
   * device was rotated in the meantime. */
  void SetRestoreResolution(float aResolution,
                            mozilla::LayoutDeviceIntSize aDisplaySize);

  /* Notify the MobileViewportManager that a reflow was requested in the
   * presShell.*/
  void RequestReflow();

  /* Notify the MobileViewportManager that the resolution on the presShell was
   * updated, and the SPCSPS needs to be updated. */
  void ResolutionUpdated();

private:
  ~MobileViewportManager();

  /* Called to compute the initial viewport on page load or before-first-paint,
   * whichever happens first. */
  void SetInitialViewport();

  /* Main helper method to update the CSS viewport and any other properties that
   * need updating. */
  void RefreshViewportSize(bool aForceAdjustResolution);

  /* Secondary main helper method to update just the SPCSPS. */
  void RefreshSPCSPS();

  /* Helper to clamp the given zoom by the min/max in the viewport info. */
  mozilla::CSSToScreenScale ClampZoom(const mozilla::CSSToScreenScale& aZoom,
                                      const nsViewportInfo& aViewportInfo);

  /* Helper to update the given resolution according to changed display and viewport widths. */
  mozilla::LayoutDeviceToLayerScale
  ScaleResolutionWithDisplayWidth(const mozilla::LayoutDeviceToLayerScale& aRes,
                                  const float& aDisplayWidthChangeRatio,
                                  const mozilla::CSSSize& aNewViewport,
                                  const mozilla::CSSSize& aOldViewport);

  /* Updates the presShell resolution and returns the new zoom. */
  mozilla::CSSToScreenScale UpdateResolution(const nsViewportInfo& aViewportInfo,
                                             const mozilla::ScreenIntSize& aDisplaySize,
                                             const mozilla::CSSSize& aViewport,
                                             const mozilla::Maybe<float>& aDisplayWidthChangeRatio);

  /* Updates the scroll-position-clamping scrollport size */
  void UpdateSPCSPS(const mozilla::ScreenIntSize& aDisplaySize,
                    const mozilla::CSSToScreenScale& aZoom);

  /* Updates the displayport margins for the presShell's root scrollable frame */
  void UpdateDisplayPortMargins();

  nsCOMPtr<nsIDocument> mDocument;
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell; // raw ref since the presShell owns this
  nsCOMPtr<nsIDOMEventTarget> mEventTarget;
  bool mIsFirstPaint;
  bool mPainted;
  mozilla::LayoutDeviceIntSize mDisplaySize;
  mozilla::CSSSize mMobileViewportSize;
  mozilla::Maybe<float> mRestoreResolution;
  mozilla::Maybe<mozilla::ScreenIntSize> mRestoreDisplaySize;
};

#endif

