/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGIMAGEFRAME_H_
#define LAYOUT_SVG_SVGIMAGEFRAME_H_

// Keep in (case-insensitive) order:
#include "mozilla/gfx/2D.h"
#include "mozilla/DisplaySVGItem.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "nsContainerFrame.h"
#include "imgINotificationObserver.h"
#include "nsIReflowCallback.h"

namespace mozilla {
class DisplaySVGImage;
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGImageFrame final : public nsIFrame,
                            public ISVGDisplayableFrame,
                            public nsIReflowCallback {
  friend nsIFrame* ::NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                                         ComputedStyle* aStyle);

  friend class DisplaySVGImage;

  bool CreateWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                               wr::IpcResourceUpdateQueue& aResources,
                               const layers::StackingContextHelper& aSc,
                               layers::RenderRootStateManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder,
                               DisplaySVGImage* aItem, bool aDryRun);

 private:
  explicit SVGImageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsIFrame(aStyle, aPresContext, kClassID),
        mReflowCallbackPosted(false),
        mForceSyncDecoding(false) {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_MAY_BE_TRANSFORMED);
    EnableVisibilityTracking();
  }

  virtual ~SVGImageFrame();

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGImageFrame)

  // ISVGDisplayableFrame interface:
  void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                imgDrawingParams& aImgParams) override;
  nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;
  SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                              uint32_t aFlags) override;
  bool IsDisplayContainer() override { return false; }

  // nsIFrame interface:
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & eSupportsContainLayoutAndPaint) {
      return false;
    }

    return nsIFrame::IsFrameOfType(aFlags & ~nsIFrame::eSVG);
  }

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  void OnVisibilityChange(
      Visibility aNewVisibility,
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void Destroy(DestroyContext&) override;

  void DidSetComputedStyle(ComputedStyle* aOldStyle) final;

  bool IsSVGTransformed(Matrix* aOwnTransforms = nullptr,
                        Matrix* aFromParentTransforms = nullptr) const override;

  bool GetIntrinsicImageDimensions(gfx::Size& aSize,
                                   AspectRatio& aAspectRatio) const;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGImage"_ns, aResult);
  }
#endif

  // nsIReflowCallback
  bool ReflowFinished() override;
  void ReflowCallbackCanceled() override;

  /// Always sync decode our image when painting if @aForce is true.
  void SetForceSyncDecoding(bool aForce) { mForceSyncDecoding = aForce; }

  // SVGImageFrame methods:
  bool IsInvisible() const;

 private:
  bool IgnoreHitTest() const;

  already_AddRefed<imgIRequest> GetCurrentRequest() const;

  gfx::Matrix GetRasterImageTransform(int32_t aNativeWidth,
                                      int32_t aNativeHeight);
  gfx::Matrix GetVectorImageTransform();
  bool TransformContextForPainting(gfxContext* aGfxContext,
                                   const gfxMatrix& aTransform);

  nsCOMPtr<imgINotificationObserver> mListener;

  nsCOMPtr<imgIContainer> mImageContainer;

  bool mReflowCallbackPosted;
  bool mForceSyncDecoding;

  friend class SVGImageListener;
};

//----------------------------------------------------------------------
// Display list item:

class DisplaySVGImage final : public DisplaySVGItem {
 public:
  DisplaySVGImage(nsDisplayListBuilder* aBuilder, SVGImageFrame* aFrame)
      : DisplaySVGItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(DisplaySVGImage);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(DisplaySVGImage)

  NS_DISPLAY_DECL_NAME("DisplaySVGImage", TYPE_SVG_IMAGE)

  // Whether this part of the SVG should be natively handled by webrender,
  // potentially becoming an "active layer" inside a blob image.
  bool ShouldBeActive(mozilla::wr::DisplayListBuilder& aBuilder,
                      mozilla::wr::IpcResourceUpdateQueue& aResources,
                      const mozilla::layers::StackingContextHelper& aSc,
                      mozilla::layers::RenderRootStateManager* aManager,
                      nsDisplayListBuilder* aDisplayListBuilder) {
    auto* frame = static_cast<SVGImageFrame*>(mFrame);
    return frame->CreateWebRenderCommands(aBuilder, aResources, aSc, aManager,
                                          aDisplayListBuilder, this,
                                          /*aDryRun=*/true);
  }

  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const mozilla::layers::StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override {
    auto* frame = static_cast<SVGImageFrame*>(mFrame);
    bool result = frame->CreateWebRenderCommands(aBuilder, aResources, aSc,
                                                 aManager, aDisplayListBuilder,
                                                 this, /*aDryRun=*/false);
    MOZ_ASSERT(result, "ShouldBeActive inconsistent with CreateWRCommands?");
    return result;
  }

  bool IsInvisible() const override {
    auto* frame = static_cast<SVGImageFrame*>(mFrame);
    return frame->IsInvisible();
  }
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGIMAGEFRAME_H_
