/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for list-item bullets */

#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"

#include "imgIContainer.h"
#include "imgINotificationObserver.h"

class imgIContainer;
class imgRequestProxy;

class nsBulletFrame;
class BulletRenderer;

class nsBulletListener final : public imgINotificationObserver {
 public:
  nsBulletListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER

  void SetFrame(nsBulletFrame* frame) { mFrame = frame; }

 private:
  virtual ~nsBulletListener();

  nsBulletFrame* mFrame;
};

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame final : public nsFrame {
  typedef mozilla::image::ImgDrawResult ImgDrawResult;

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsBulletFrame)
#ifdef DEBUG
  NS_DECL_QUERYFRAME
#endif

  explicit nsBulletFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsFrame(aStyle, aPresContext, kClassID),
        mPadding(GetWritingMode()),
        mIntrinsicSize(GetWritingMode()),
        mRequestRegistered(false) {}

  virtual ~nsBulletFrame();

  NS_IMETHOD Notify(imgIRequest* aRequest, int32_t aType,
                    const nsIntRect* aData);

  // nsIFrame
  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;
  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;
  virtual void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;
  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;
  void AddInlineMinISize(gfxContext* aRenderingContext,
                         nsIFrame::InlineMinISizeData* aData) override;
  void AddInlinePrefISize(gfxContext* aRenderingContext,
                          nsIFrame::InlinePrefISizeData* aData) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & (eSupportsCSSTransforms | eSupportsContainLayoutAndPaint)) {
      return false;
    }
    return nsFrame::IsFrameOfType(aFlags & ~nsIFrame::eLineParticipant);
  }

  // nsBulletFrame

  /* get list item text, with prefix & suffix */
  static void GetListItemText(mozilla::CounterStyle*, mozilla::WritingMode,
                              int32_t aOrdinal, nsAString& aResult);

#ifdef ACCESSIBILITY
  void GetSpokenText(nsAString& aText);
#endif

  Maybe<BulletRenderer> CreateBulletRenderer(gfxContext& aRenderingContext,
                                             nsPoint aPt);
  ImgDrawResult PaintBullet(gfxContext& aRenderingContext, nsPoint aPt,
                            const nsRect& aDirtyRect, uint32_t aFlags,
                            bool aDisableSubpixelAA);

  virtual bool IsEmpty() override;
  virtual bool IsSelfEmpty() override;

  // XXXmats note that this method returns a non-standard baseline that includes
  // the ::marker block-start margin.  New code should probably use
  // GetNaturalBaselineBOffset instead, which returns a normal baseline offset
  // as documented in nsIFrame.h.
  virtual nscoord GetLogicalBaseline(
      mozilla::WritingMode aWritingMode) const override;

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override;

  float GetFontSizeInflation() const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & BULLET_FRAME_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  int32_t Ordinal() const { return mOrdinal; }
  void SetOrdinal(int32_t aOrdinal, bool aNotify);

  already_AddRefed<imgIContainer> GetImage() const;

 protected:
  nsresult OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);

  void AppendSpacingToPadding(nsFontMetrics* aFontMetrics,
                              mozilla::LogicalMargin* aPadding);
  void GetDesiredSize(nsPresContext* aPresContext,
                      gfxContext* aRenderingContext, ReflowOutput& aMetrics,
                      float aFontSizeInflation,
                      mozilla::LogicalMargin* aPadding);

  void GetLoadGroup(nsPresContext* aPresContext, nsILoadGroup** aLoadGroup);
  mozilla::dom::Document* GetOurCurrentDoc() const;

  mozilla::LogicalMargin mPadding;
  RefPtr<imgRequestProxy> mImageRequest;
  RefPtr<nsBulletListener> mListener;

  mozilla::LogicalSize mIntrinsicSize;

 private:
  mozilla::CounterStyle* ResolveCounterStyle();
  nscoord GetListStyleAscent() const;
  void RegisterImageRequest(bool aKnownToBeAnimated);
  void DeregisterAndCancelImageRequest();

  // Requires being set via SetOrdinal.
  int32_t mOrdinal = 0;

  // This is a boolean flag indicating whether or not the current image request
  // has been registered with the refresh driver.
  bool mRequestRegistered : 1;
};

#endif /* nsBulletFrame_h___ */
