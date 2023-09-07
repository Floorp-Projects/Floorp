/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MiddleCroppingBlockFrame_h
#define mozilla_MiddleCroppingBlockFrame_h

#include "nsBlockFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsQueryFrame.h"

namespace mozilla {

// A block frame that implements a simplistic version of middle-cropping. Note
// that this is not fully l10n aware or complex-writing-system-friendly, so it's
// generally mostly useful for filenames / urls / etc.
class MiddleCroppingBlockFrame : public nsBlockFrame,
                                 public nsIAnonymousContentCreator {
 public:
  virtual void GetUncroppedValue(nsAString&) = 0;
  void UpdateDisplayedValueToUncroppedValue(bool aNotify);

  NS_DECL_QUERYFRAME_TARGET(MiddleCroppingBlockFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_ABSTRACT_FRAME(MiddleCroppingBlockFrame)

 protected:
  MiddleCroppingBlockFrame(ComputedStyle*, nsPresContext*, ClassID);

  ~MiddleCroppingBlockFrame();

  void Reflow(nsPresContext*, ReflowOutput&, const ReflowInput&,
              nsReflowStatus&) override;

  nscoord GetMinISize(gfxContext*) override;
  nscoord GetPrefISize(gfxContext*) override;

  /**
   * Crop aText to fit inside aWidth using the styles of aFrame.
   * @return true if aText was modified
   */
  bool CropTextToWidth(gfxContext& aRenderingContext, nscoord aWidth,
                       nsString& aText) const;

  nsresult CreateAnonymousContent(nsTArray<ContentInfo>&) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>&,
                                uint32_t aFilter) override;

  /**
   * Updates the displayed value by using aValue.
   */
  void UpdateDisplayedValue(const nsAString& aValue, bool aIsCropped,
                            bool aNotify);
  void Destroy(DestroyContext&) override;

  RefPtr<dom::Text> mTextNode;
  bool mCropped = false;
};

}  // namespace mozilla

#endif
