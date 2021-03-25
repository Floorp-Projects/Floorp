/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "mozilla/Attributes.h"
#include "nsIFrame.h"

class imgIContainer;
class nsBulletFrame;
class nsFontMetrics;
class BulletRenderer;
class nsFontMetrics;

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame final : public nsIFrame {
  typedef mozilla::image::ImgDrawResult ImgDrawResult;

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsBulletFrame)
#ifdef DEBUG
  NS_DECL_QUERYFRAME
#endif

  explicit nsBulletFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsIFrame(aStyle, aPresContext, kClassID), mPadding(GetWritingMode()) {}

  virtual ~nsBulletFrame();

  // nsIFrame
  void BuildDisplayList(nsDisplayListBuilder*,
                        const nsDisplayListSet&) override;
  void DidSetComputedStyle(ComputedStyle* aOldStyle) override;
#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
              const ReflowInput&, nsReflowStatus&) override;
  nscoord GetMinISize(gfxContext*) override;
  nscoord GetPrefISize(gfxContext*) override;
  void AddInlineMinISize(gfxContext*, nsIFrame::InlineMinISizeData*) override;
  void AddInlinePrefISize(gfxContext*, nsIFrame::InlinePrefISizeData*) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & (eSupportsCSSTransforms | eSupportsContainLayoutAndPaint)) {
      return false;
    }
    return nsIFrame::IsFrameOfType(aFlags & ~nsIFrame::eLineParticipant);
  }

  // nsBulletFrame

  /* get list item text, with prefix & suffix */
  static void GetListItemText(mozilla::CounterStyle*, mozilla::WritingMode,
                              int32_t aOrdinal, nsAString& aResult);

#ifdef ACCESSIBILITY
  void GetSpokenText(nsAString& aText);
#endif

  Maybe<BulletRenderer> CreateBulletRenderer(gfxContext& aRenderingContext,
                                             nsDisplayListBuilder* aBuilder,
                                             nsPoint aPt,
                                             ImgDrawResult* aOutDrawResult);
  ImgDrawResult PaintBullet(gfxContext& aRenderingContext,
                            nsDisplayListBuilder* aBuilder, nsPoint aPt,
                            const nsRect& aDirtyRect, bool aDisableSubpixelAA);

  bool IsEmpty() override;
  bool IsSelfEmpty() override;

  // XXXmats note that this method returns a non-standard baseline that includes
  // the ::marker block-start margin.  New code should probably use
  // GetNaturalBaselineBOffset instead, which returns a normal baseline offset
  // as documented in nsIFrame.h.
  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override;

  float GetFontSizeInflation() const;
  bool HasFontSizeInflation() const {
    return HasAnyStateBits(BULLET_FRAME_HAS_FONT_INFLATION);
  }
  void SetFontSizeInflation(float aInflation);

  int32_t Ordinal() const { return mOrdinal; }
  void SetOrdinal(int32_t aOrdinal, bool aNotify);

  already_AddRefed<imgIContainer> GetImage() const;

 protected:
  void AppendSpacingToPadding(nsFontMetrics* aFontMetrics,
                              mozilla::LogicalMargin* aPadding);
  void GetDesiredSize(nsPresContext* aPresContext,
                      gfxContext* aRenderingContext, ReflowOutput& aMetrics,
                      float aFontSizeInflation,
                      mozilla::LogicalMargin* aPadding);

  mozilla::LogicalMargin mPadding;

 private:
  mozilla::CounterStyle* ResolveCounterStyle();
  nscoord GetListStyleAscent() const;

  // Requires being set via SetOrdinal.
  int32_t mOrdinal = 0;
};

#endif /* nsBulletFrame_h___ */
