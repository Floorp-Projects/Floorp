/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGOUTERSVGFRAME_H_
#define LAYOUT_SVG_SVGOUTERSVGFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/ISVGSVGFrame.h"
#include "mozilla/SVGContainerFrame.h"

class gfxContext;

namespace mozilla {
class AutoSVGViewHandler;
class SVGFragmentIdentifier;
class PresShell;
}  // namespace mozilla

nsContainerFrame* NS_NewSVGOuterSVGFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle);
nsContainerFrame* NS_NewSVGOuterSVGAnonChildFrame(
    mozilla::PresShell* aPresShell, mozilla::ComputedStyle* aStyle);

namespace mozilla {

////////////////////////////////////////////////////////////////////////
// SVGOuterSVGFrame class

class SVGOuterSVGFrame final : public SVGDisplayContainerFrame,
                               public ISVGSVGFrame {
  using imgDrawingParams = image::imgDrawingParams;

  friend nsContainerFrame* ::NS_NewSVGOuterSVGFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);
  friend class AutoSVGViewHandler;
  friend class SVGFragmentIdentifier;

 protected:
  explicit SVGOuterSVGFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGOuterSVGFrame)

  // nsIFrame:
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  IntrinsicSize GetIntrinsicSize() override;
  AspectRatio GetIntrinsicRatio() const override;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, WritingMode aWritingMode,
      const LogicalSize& aCBSize, nscoord aAvailableISize,
      const LogicalSize& aMargin, const LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      ComputeSizeFlags aFlags) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void DidReflow(nsPresContext* aPresContext,
                 const ReflowInput* aReflowInput) override;

  void UnionChildOverflow(mozilla::OverflowAreas& aOverflowAreas) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    return SVGDisplayContainerFrame::IsFrameOfType(
        aFlags &
        ~(eSupportsContainLayoutAndPaint | eReplaced | eReplacedSizing));
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGOuterSVG"_ns, aResult);
  }
#endif

  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

  void Destroy(DestroyContext&) override;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  nsContainerFrame* GetContentInsertionFrame() override {
    // Any children must be added to our single anonymous inner frame kid.
    MOZ_ASSERT(
        PrincipalChildList().FirstChild() &&
            PrincipalChildList().FirstChild()->IsSVGOuterSVGAnonChildFrame(),
        "Where is our anonymous child?");
    return PrincipalChildList().FirstChild()->GetContentInsertionFrame();
  }

  bool IsSVGTransformed(Matrix* aOwnTransform,
                        Matrix* aFromParentTransform) const override;

  // Return our anonymous box child.
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

  // ISVGSVGFrame interface:
  void NotifyViewportOrTransformChanged(uint32_t aFlags) override;

  // ISVGDisplayableFrame methods:
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;

  // SVGContainerFrame methods:
  gfxMatrix GetCanvasTM() override;

  bool HasChildrenOnlyTransform(Matrix* aTransform) const override {
    // Our anonymous wrapper child must claim our children-only transforms as
    // its own so that our real children (the frames it wraps) are transformed
    // by them, and we must pretend we don't have any children-only transforms
    // so that our anonymous child is _not_ transformed by them.
    return false;
  }

  /**
   * Return true only if the height is unspecified (defaulting to 100%) or else
   * the height is explicitly set to a percentage value no greater than 100%.
   */
  bool VerticalScrollbarNotNeeded() const;

  bool IsCallingReflowSVG() const { return mCallingReflowSVG; }

 protected:
  /* Returns true if our content is the document element and our document is
   * being used as an image.
   */
  bool IsRootOfImage();
  float ComputeFullZoom() const;

  void MaybeSendIntrinsicSizeAndRatioToEmbedder();
  void MaybeSendIntrinsicSizeAndRatioToEmbedder(Maybe<IntrinsicSize>,
                                                Maybe<AspectRatio>);

  float mFullZoom = 1.0f;

  bool mCallingReflowSVG = false;
  bool mIsRootContent = false;
  bool mIsInObjectOrEmbed = false;
  bool mIsInIframe = false;
};

////////////////////////////////////////////////////////////////////////
// SVGOuterSVGAnonChildFrame class

/**
 * SVGOuterSVGFrames have a single direct child that is an instance of this
 * class, and which is used to wrap their real child frames. Such anonymous
 * wrapper frames created from this class exist because SVG frames need their
 * GetPosition() offset to be their offset relative to "user space" (in app
 * units) so that they can play nicely with nsDisplayTransform. This is fine
 * for all SVG frames except for direct children of an SVGOuterSVGFrame,
 * since an SVGOuterSVGFrame can have CSS border and padding (unlike other
 * SVG frames). The direct children can't include the offsets due to any such
 * border/padding in their mRects since that would break nsDisplayTransform,
 * but not including these offsets would break other parts of the Mozilla code
 * that assume a frame's mRect contains its border-box-to-parent-border-box
 * offset, in particular nsIFrame::GetOffsetTo and the functions that depend on
 * it. Wrapping an SVGOuterSVGFrame's children in an instance of this class
 * with its GetPosition() set to its SVGOuterSVGFrame's border/padding offset
 * keeps both nsDisplayTransform and nsIFrame::GetOffsetTo happy.
 *
 * The reason that this class inherit from SVGDisplayContainerFrame rather
 * than simply from nsContainerFrame is so that we can avoid having special
 * handling for these inner wrappers in multiple parts of the SVG code. For
 * example, the implementations of IsSVGTransformed and GetCanvasTM assume
 * SVGContainerFrame instances all the way up to the SVGOuterSVGFrame.
 */
class SVGOuterSVGAnonChildFrame final : public SVGDisplayContainerFrame {
  friend nsContainerFrame* ::NS_NewSVGOuterSVGAnonChildFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  explicit SVGOuterSVGAnonChildFrame(ComputedStyle* aStyle,
                                     nsPresContext* aPresContext)
      : SVGDisplayContainerFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGOuterSVGAnonChildFrame)

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGOuterSVGAnonChild"_ns, aResult);
  }
#endif

  bool IsSVGTransformed(Matrix* aOwnTransform,
                        Matrix* aFromParentTransform) const override;

  // SVGContainerFrame methods:
  gfxMatrix GetCanvasTM() override {
    // GetCanvasTM returns the transform from an SVG frame to the frame's
    // SVGOuterSVGFrame's content box, so we do not include any x/y offset
    // set on us for any CSS border or padding on our SVGOuterSVGFrame.
    return static_cast<SVGOuterSVGFrame*>(GetParent())->GetCanvasTM();
  }
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGOUTERSVGFRAME_H_
