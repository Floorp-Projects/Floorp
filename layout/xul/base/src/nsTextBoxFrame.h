/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTextBoxFrame_h___
#define nsTextBoxFrame_h___

#include "nsLeafBoxFrame.h"

class nsAccessKeyInfo;
class nsAsyncAccesskeyUpdate;

typedef nsLeafBoxFrame nsTextBoxFrameSuper;
class nsTextBoxFrame : public nsTextBoxFrameSuper
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual void MarkIntrinsicWidthsDirty();

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsIFrame* NS_NewTextBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  void UpdateAttributes(nsIAtom*         aAttribute,
                        bool&          aResize,
                        bool&          aRedraw);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual ~nsTextBoxFrame();

  void PaintTitle(nsRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsPoint              aPt,
                  const nscolor*       aOverrideColor);

  nsRect GetComponentAlphaBounds();

  virtual bool ComputesOwnOverflowArea();

protected:
  friend class nsAsyncAccesskeyUpdate;
  friend class nsDisplayXULTextBox;
  // Should be called only by nsAsyncAccesskeyUpdate.
  // Returns true if accesskey was updated.
  bool UpdateAccesskey(nsWeakFrame& aWeakThis);
  void UpdateAccessTitle();
  void UpdateAccessIndex();

  // REVIEW: SORRY! Couldn't resist devirtualizing these
  void LayoutTitle(nsPresContext*      aPresContext,
                   nsRenderingContext& aRenderingContext,
                   const nsRect&        aRect);

  void CalculateUnderline(nsRenderingContext& aRenderingContext);

  void CalcTextSize(nsBoxLayoutState& aBoxLayoutState);

  void CalcDrawRect(nsRenderingContext &aRenderingContext);

  nsTextBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  nscoord CalculateTitleForWidth(nsPresContext*      aPresContext,
                                 nsRenderingContext& aRenderingContext,
                                 nscoord              aWidth);

  void GetTextSize(nsPresContext*      aPresContext,
                   nsRenderingContext& aRenderingContext,
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
