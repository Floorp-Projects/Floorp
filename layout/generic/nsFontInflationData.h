/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per-block-formatting-context manager of font size inflation for pan and zoom
 * UI. */

#ifndef nsFontInflationData_h_
#define nsFontInflationData_h_

#include "nsContainerFrame.h"

class nsFontInflationData {
  using ReflowInput = mozilla::ReflowInput;

 public:
  static nsFontInflationData* FindFontInflationDataFor(const nsIFrame* aFrame);

  // Returns whether the usable width changed (which requires the
  // caller to mark its descendants dirty)
  static bool UpdateFontInflationDataISizeFor(const ReflowInput& aReflowInput);

  static void MarkFontInflationDataTextDirty(nsIFrame* aFrame);

  bool InflationEnabled() {
    if (mTextDirty) {
      ScanText();
    }
    return mInflationEnabled;
  }

  nscoord UsableISize() const { return mUsableISize; }

 private:
  explicit nsFontInflationData(nsIFrame* aBFCFrame);

  nsFontInflationData(const nsFontInflationData&) = delete;
  void operator=(const nsFontInflationData&) = delete;

  void UpdateISize(const ReflowInput& aReflowInput);
  enum SearchDirection { eFromStart, eFromEnd };
  static nsIFrame* FindEdgeInflatableFrameIn(nsIFrame* aFrame,
                                             SearchDirection aDirection);

  void MarkTextDirty() { mTextDirty = true; }
  void ScanText();
  // Scan text in the subtree rooted at aFrame.  Increment mTextAmount
  // by multiplying the number of characters found by the font size
  // (yielding the inline-size that would be occupied by the characters if
  // they were all em squares).  But stop scanning if mTextAmount
  // crosses mTextThreshold.
  void ScanTextIn(nsIFrame* aFrame);

  static const nsIFrame* FlowRootFor(const nsIFrame* aFrame) {
    while (!(aFrame->GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT)) {
      aFrame = aFrame->GetParent();
    }
    return aFrame;
  }

  nsIFrame* mBFCFrame;
  nscoord mUsableISize;
  nscoord mTextAmount, mTextThreshold;
  bool mInflationEnabled;  // for this BFC
  bool mTextDirty;
};

#endif /* !defined(nsFontInflationData_h_) */
