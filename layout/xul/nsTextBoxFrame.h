/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

typedef nsLeafBoxFrame nsTextBoxFrameSuper;
class nsTextBoxFrame : public nsTextBoxFrameSuper
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsTextBoxFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) override;
  virtual void MarkIntrinsicISizesDirty() override;

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsIFrame* NS_NewTextBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         asPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult AttributeChanged(int32_t         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    int32_t         aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void UpdateAttributes(nsIAtom*         aAttribute,
                        bool&          aResize,
                        bool&          aRedraw);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual ~nsTextBoxFrame();

  void PaintTitle(nsRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsPoint              aPt,
                  const nscolor*       aOverrideColor);

  nsRect GetComponentAlphaBounds();

  virtual bool ComputesOwnOverflowArea() override;

  void GetCroppedTitle(nsString& aTitle) const { aTitle = mCroppedTitle; }

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) override;

protected:
  friend class nsAsyncAccesskeyUpdate;
  friend class nsDisplayXULTextBox;
  // Should be called only by nsAsyncAccesskeyUpdate.
  // Returns true if accesskey was updated.
  bool UpdateAccesskey(nsWeakFrame& aWeakThis);
  void UpdateAccessTitle();
  void UpdateAccessIndex();

  // Recompute our title, ignoring the access key but taking into
  // account text-transform.
  void RecomputeTitle();

  // REVIEW: SORRY! Couldn't resist devirtualizing these
  void LayoutTitle(nsPresContext*      aPresContext,
                   nsRenderingContext& aRenderingContext,
                   const nsRect&        aRect);

  void CalculateUnderline(DrawTarget* aDrawTarget, nsFontMetrics& aFontMetrics);

  void CalcTextSize(nsBoxLayoutState& aBoxLayoutState);

  void CalcDrawRect(nsRenderingContext &aRenderingContext);

  explicit nsTextBoxFrame(nsStyleContext* aContext);

  nscoord CalculateTitleForWidth(nsRenderingContext& aRenderingContext,
                                 nscoord              aWidth);

  void GetTextSize(nsRenderingContext& aRenderingContext,
                   const nsString&      aString,
                   nsSize&              aSize,
                   nscoord&             aAscent);

  nsresult RegUnregAccessKey(bool aDoReg);

private:

  bool AlwaysAppendAccessKey();
  bool InsertSeparatorBeforeAccessKey();

  void DrawText(nsRenderingContext& aRenderingContext,
                const nsRect&       aDirtyRect,
                const nsRect&       aTextRect,
                const nscolor*      aOverrideColor);

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

}; // class nsTextBoxFrame

#endif /* nsTextBoxFrame_h___ */
