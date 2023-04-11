/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGIMAGEFRAME_H_
#define LAYOUT_SVG_SVGIMAGEFRAME_H_

// Keep in (case-insensitive) order:
#include "mozilla/gfx/2D.h"
#include "mozilla/SVGGeometryFrame.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "nsContainerFrame.h"
#include "imgINotificationObserver.h"
#include "nsIReflowCallback.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGImageFrame final : public SVGGeometryFrame, public nsIReflowCallback {
  friend nsIFrame* ::NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                                         ComputedStyle* aStyle);

  bool CreateWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                               wr::IpcResourceUpdateQueue& aResources,
                               const layers::StackingContextHelper& aSc,
                               layers::RenderRootStateManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder,
                               DisplaySVGGeometry* aItem,
                               bool aDryRun) override;

 protected:
  explicit SVGImageFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGGeometryFrame(aStyle, aPresContext, kClassID),
        mReflowCallbackPosted(false),
        mForceSyncDecoding(false) {
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

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  // nsIFrame interface:
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  void OnVisibilityChange(
      Visibility aNewVisibility,
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;
  void DidSetComputedStyle(ComputedStyle* aOldStyle) final;

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

 private:
  uint16_t GetHitTestFlags();

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

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGIMAGEFRAME_H_
