/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGOUTERSVGFRAME_H__
#define __NS_SVGOUTERSVGFRAME_H__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsISVGSVGFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsRegion.h"

class gfxContext;
class nsSVGForeignObjectFrame;

namespace mozilla {
class PresShell;
}  // namespace mozilla

////////////////////////////////////////////////////////////////////////
// nsSVGOuterSVGFrame class

class nsSVGOuterSVGFrame final : public nsSVGDisplayContainerFrame,
                                 public nsISVGSVGFrame {
  typedef mozilla::image::imgDrawingParams imgDrawingParams;

  friend nsContainerFrame* NS_NewSVGOuterSVGFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit nsSVGOuterSVGFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext);

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsSVGOuterSVGFrame)

#ifdef DEBUG
  ~nsSVGOuterSVGFrame() {
    NS_ASSERTION(!mForeignObjectHash || mForeignObjectHash->Count() == 0,
                 "foreignObject(s) still registered!");
  }
#endif

  // nsIFrame:
  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  virtual mozilla::IntrinsicSize GetIntrinsicSize() override;
  virtual mozilla::AspectRatio GetIntrinsicRatio() override;

  virtual mozilla::LogicalSize ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWritingMode,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin, const mozilla::LogicalSize& aBorder,
      const mozilla::LogicalSize& aPadding, ComputeSizeFlags aFlags) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual void DidReflow(nsPresContext* aPresContext,
                         const ReflowInput* aReflowInput) override;

  virtual void UnionChildOverflow(nsOverflowAreas& aOverflowAreas) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsSVGDisplayContainerFrame::IsFrameOfType(
        aFlags &
        ~(eSupportsContainLayoutAndPaint | eReplaced | eReplacedSizing));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGOuterSVG"), aResult);
  }
#endif

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual nsContainerFrame* GetContentInsertionFrame() override {
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

  // nsISVGSVGFrame interface:
  virtual void NotifyViewportOrTransformChanged(uint32_t aFlags) override;

  // nsSVGDisplayableFrame methods:
  virtual void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams,
                        const nsIntRect* aDirtyRect = nullptr) override;
  virtual SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                                      uint32_t aFlags) override;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  /* Methods to allow descendant nsSVGForeignObjectFrame frames to register and
   * unregister themselves with their nearest nsSVGOuterSVGFrame ancestor. This
   * is temporary until display list based invalidation is impleented for SVG.
   * Maintaining a list of our foreignObject descendants allows us to search
   * them for areas that need to be invalidated, without having to also search
   * the SVG frame tree for foreignObjects. This is important so that bug 539356
   * does not slow down SVG in general (only foreignObjects, until bug 614732 is
   * fixed).
   */
  void RegisterForeignObject(nsSVGForeignObjectFrame* aFrame);
  void UnregisterForeignObject(nsSVGForeignObjectFrame* aFrame);

  virtual bool HasChildrenOnlyTransform(Matrix* aTransform) const override {
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

  void InvalidateSVG(const nsRegion& aRegion) {
    if (!aRegion.IsEmpty()) {
      mInvalidRegion.Or(mInvalidRegion, aRegion);
      InvalidateFrame();
    }
  }

  void ClearInvalidRegion() { mInvalidRegion.SetEmpty(); }

  const nsRegion& GetInvalidRegion() {
    nsRect rect;
    if (!IsInvalid(rect)) {
      mInvalidRegion.SetEmpty();
    }
    return mInvalidRegion;
  }

  nsRegion FindInvalidatedForeignObjectFrameChildren(nsIFrame* aFrame);

 protected:
  bool mCallingReflowSVG;

  /* Returns true if our content is the document element and our document is
   * embedded in an HTML 'object' or 'embed' element. Set
   * aEmbeddingFrame to obtain the nsIFrame for the embedding HTML element.
   */
  bool IsRootOfReplacedElementSubDoc(nsIFrame** aEmbeddingFrame = nullptr);

  /* Returns true if our content is the document element and our document is
   * being used as an image.
   */
  bool IsRootOfImage();

  // This is temporary until display list based invalidation is implemented for
  // SVG.
  // A hash-set containing our nsSVGForeignObjectFrame descendants. Note we use
  // a hash-set to avoid the O(N^2) behavior we'd get tearing down an SVG frame
  // subtree if we were to use a list (see bug 381285 comment 20).
  nsAutoPtr<nsTHashtable<nsPtrHashKey<nsSVGForeignObjectFrame>>>
      mForeignObjectHash;

  nsRegion mInvalidRegion;

  float mFullZoom;

  bool mViewportInitialized;
  bool mIsRootContent;

 private:
  template <typename... Args>
  bool IsContainingWindowElementOfType(nsIFrame** aContainingWindowFrame,
                                       Args... aArgs) const;
};

////////////////////////////////////////////////////////////////////////
// nsSVGOuterSVGAnonChildFrame class

/**
 * nsSVGOuterSVGFrames have a single direct child that is an instance of this
 * class, and which is used to wrap their real child frames. Such anonymous
 * wrapper frames created from this class exist because SVG frames need their
 * GetPosition() offset to be their offset relative to "user space" (in app
 * units) so that they can play nicely with nsDisplayTransform. This is fine
 * for all SVG frames except for direct children of an nsSVGOuterSVGFrame,
 * since an nsSVGOuterSVGFrame can have CSS border and padding (unlike other
 * SVG frames). The direct children can't include the offsets due to any such
 * border/padding in their mRects since that would break nsDisplayTransform,
 * but not including these offsets would break other parts of the Mozilla code
 * that assume a frame's mRect contains its border-box-to-parent-border-box
 * offset, in particular nsIFrame::GetOffsetTo and the functions that depend on
 * it. Wrapping an nsSVGOuterSVGFrame's children in an instance of this class
 * with its GetPosition() set to its nsSVGOuterSVGFrame's border/padding offset
 * keeps both nsDisplayTransform and nsIFrame::GetOffsetTo happy.
 *
 * The reason that this class inherit from nsSVGDisplayContainerFrame rather
 * than simply from nsContainerFrame is so that we can avoid having special
 * handling for these inner wrappers in multiple parts of the SVG code. For
 * example, the implementations of IsSVGTransformed and GetCanvasTM assume
 * nsSVGContainerFrame instances all the way up to the nsSVGOuterSVGFrame.
 */
class nsSVGOuterSVGAnonChildFrame final : public nsSVGDisplayContainerFrame {
  friend nsContainerFrame* NS_NewSVGOuterSVGAnonChildFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  explicit nsSVGOuterSVGAnonChildFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext)
      : nsSVGDisplayContainerFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGOuterSVGAnonChildFrame)

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGOuterSVGAnonChild"), aResult);
  }
#endif

  bool IsSVGTransformed(Matrix* aOwnTransform,
                        Matrix* aFromParentTransform) const override;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override {
    // GetCanvasTM returns the transform from an SVG frame to the frame's
    // nsSVGOuterSVGFrame's content box, so we do not include any x/y offset
    // set on us for any CSS border or padding on our nsSVGOuterSVGFrame.
    return static_cast<nsSVGOuterSVGFrame*>(GetParent())->GetCanvasTM();
  }
};

#endif
