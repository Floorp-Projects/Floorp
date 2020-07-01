/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGIMAGEFRAME_H__
#define __NS_SVGIMAGEFRAME_H__

// Keep in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/2D.h"
#include "imgIContainer.h"
#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"
#include "imgINotificationObserver.h"
#include "SVGObserverUtils.h"
#include "nsSVGUtils.h"
#include "SVGContentUtils.h"
#include "SVGGeometryFrame.h"
#include "SVGImageContext.h"
#include "mozilla/dom/SVGImageElement.h"
#include "nsIReflowCallback.h"
#include "mozilla/Unused.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGImageFrame final : public SVGGeometryFrame, public nsIReflowCallback {
  friend nsIFrame* ::NS_NewSVGImageFrame(mozilla::PresShell* aPresShell,
                                         ComputedStyle* aStyle);

  virtual bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const layers::StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder, DisplaySVGGeometry* aItem,
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

  // nsSVGDisplayableFrame interface:
  virtual void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams,
                        const nsIntRect* aDirtyRect = nullptr) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual void ReflowSVG() override;

  // SVGGeometryFrame methods:
  virtual uint16_t GetHitTestFlags() override;

  // nsIFrame interface:
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  void OnVisibilityChange(
      Visibility aNewVisibility,
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;
  void DidSetComputedStyle(ComputedStyle* aOldStyle) final;

  bool GetIntrinsicImageDimensions(gfx::Size& aSize,
                                   AspectRatio& aAspectRatio) const;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGImage"_ns, aResult);
  }
#endif

  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

  /// Always sync decode our image when painting if @aForce is true.
  void SetForceSyncDecoding(bool aForce) { mForceSyncDecoding = aForce; }

 private:
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

#endif  // __NS_SVGIMAGEFRAME_H__
