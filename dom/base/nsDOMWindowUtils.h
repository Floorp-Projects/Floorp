/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMWindowUtils_h_
#define nsDOMWindowUtils_h_

#include "nsWeakReference.h"

#include "nsIDOMWindowUtils.h"
#include "mozilla/Attributes.h"

class nsGlobalWindow;
class nsIPresShell;

class nsDOMWindowUtils MOZ_FINAL : public nsIDOMWindowUtils,
                                   public nsSupportsWeakReference
{
public:
  nsDOMWindowUtils(nsGlobalWindow *aWindow);
  ~nsDOMWindowUtils();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWINDOWUTILS

protected:
  nsWeakPtr mWindow;

  // If aOffset is non-null, it gets filled in with the offset of the root
  // frame of our window to the nearest widget in the app units of our window.
  // Add this offset to any event offset we're given to make it relative to the
  // widget returned by GetWidget.
  nsIWidget* GetWidget(nsPoint* aOffset = nullptr);
  nsIWidget* GetWidgetForElement(nsIDOMElement* aElement);

  nsIPresShell* GetPresShell();
  nsPresContext* GetPresContext();

  NS_IMETHOD SendMouseEventCommon(const nsAString& aType,
                                  float aX,
                                  float aY,
                                  int32_t aButton,
                                  int32_t aClickCount,
                                  int32_t aModifiers,
                                  bool aIgnoreRootScrollFrame,
                                  float aPressure,
                                  unsigned short aInputSourceArg,
                                  bool aToWindow,
                                  bool *aPreventDefault,
                                  bool aIsSynthesized);

  NS_IMETHOD SendTouchEventCommon(const nsAString& aType,
                                  uint32_t* aIdentifiers,
                                  int32_t* aXs,
                                  int32_t* aYs,
                                  uint32_t* aRxs,
                                  uint32_t* aRys,
                                  float* aRotationAngles,
                                  float* aForces,
                                  uint32_t aCount,
                                  int32_t aModifiers,
                                  bool aIgnoreRootScrollFrame,
                                  bool aToWindow,
                                  bool* aPreventDefault);


  static mozilla::Modifiers GetWidgetModifiers(int32_t aModifiers);
};

#endif
