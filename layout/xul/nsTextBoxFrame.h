/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTextBoxFrame_h___
#define nsTextBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsLeafBoxFrame.h"

class nsAccessKeyInfo;
class nsAsyncAccesskeyUpdate;
class nsFontMetrics;

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsTextBoxFrame final : public nsLeafBoxFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsTextBoxFrame)

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;
  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;
  virtual void MarkIntrinsicISizesDirty() override;

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter, CropAuto };

  friend nsIFrame* NS_NewTextBoxFrame(mozilla::PresShell* aPresShell,
                                      ComputedStyle* aStyle);

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* asPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void UpdateAttributes(nsAtom* aAttribute, bool& aResize, bool& aRedraw);

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual ~nsTextBoxFrame();

  void PaintTitle(gfxContext& aRenderingContext, const nsRect& aDirtyRect,
                  nsPoint aPt, const nscolor* aOverrideColor);

  nsRect GetComponentAlphaBounds() const;

  virtual bool XULComputesOwnOverflowArea() override;

  void GetCroppedTitle(nsString& aTitle) const { aTitle = mCroppedTitle; }

  virtual void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

 protected:
  friend class nsAsyncAccesskeyUpdate;
  friend class nsDisplayXULTextBox;
  // Should be called only by nsAsyncAccesskeyUpdate.
  // Returns true if accesskey was updated.
  bool UpdateAccesskey(WeakFrame& aWeakThis);
  void UpdateAccessTitle();
  void UpdateAccessIndex();

  // Recompute our title, ignoring the access key but taking into
  // account text-transform.
  void RecomputeTitle();

  // REVIEW: SORRY! Couldn't resist devirtualizing these
  void LayoutTitle(nsPresContext* aPresContext, gfxContext& aRenderingContext,
                   const nsRect& aRect);

  void CalculateUnderline(DrawTarget* aDrawTarget, nsFontMetrics& aFontMetrics);

  void CalcTextSize(nsBoxLayoutState& aBoxLayoutState);

  void CalcDrawRect(gfxContext& aRenderingContext);

  explicit nsTextBoxFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

  nscoord CalculateTitleForWidth(gfxContext& aRenderingContext, nscoord aWidth);

  void GetTextSize(gfxContext& aRenderingContext, const nsString& aString,
                   nsSize& aSize, nscoord& aAscent);

  nsresult RegUnregAccessKey(bool aDoReg);

 private:
  bool AlwaysAppendAccessKey();
  bool InsertSeparatorBeforeAccessKey();

  void DrawText(gfxContext& aRenderingContext, const nsRect& aDirtyRect,
                const nsRect& aTextRect, const nscolor* aOverrideColor);

  nsString mTitle;
  nsString mCroppedTitle;
  nsString mAccessKey;
  nsSize mTextSize;
  nsRect mTextDrawRect;
  nsAccessKeyInfo* mAccessKeyInfo;

  CroppingStyle mCropType;
  nscoord mAscent;
  bool mNeedsRecalc;
  bool mNeedsReflowCallback;

  static bool gAlwaysAppendAccessKey;
  static bool gAccessKeyPrefInitialized;
  static bool gInsertSeparatorBeforeAccessKey;
  static bool gInsertSeparatorPrefInitialized;

};  // class nsTextBoxFrame

#endif /* nsTextBoxFrame_h___ */
