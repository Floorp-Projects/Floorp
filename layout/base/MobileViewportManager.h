/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MobileViewportManager_h_
#define MobileViewportManager_h_

#include "mozilla/Maybe.h"
#include "nsIDOMEventListener.h"
#include "nsIDocument.h"
#include "nsIObserver.h"
#include "Units.h"

class nsIDOMEventTarget;
class nsIDocument;
class nsIPresShell;

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

  /* Notify the MobileViewportManager that a reflow was requested in the
   * presShell.*/
  void RequestReflow();

private:
  ~MobileViewportManager();

  /* Main helper method to update the CSS viewport and any other properties that
   * need updating. */
  void RefreshViewportSize(bool aForceAdjustResolution);

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
  mozilla::LayoutDeviceIntSize mDisplaySize;
  mozilla::CSSSize mMobileViewportSize;
};

#endif

