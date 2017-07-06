/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <video> element */

#ifndef nsVideoFrame_h___
#define nsVideoFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsTArrayForwardDeclare.h"
#include "FrameLayerBuilder.h"

namespace mozilla {
namespace layers {
class Layer;
class LayerManager;
} // namespace layers
} // namespace mozilla

class nsAString;
class nsPresContext;
class nsDisplayItem;

class nsVideoFrame : public nsContainerFrame
                   , public nsIAnonymousContentCreator
{
public:
  template <typename T> using Maybe = mozilla::Maybe<T>;
  using Nothing = mozilla::Nothing;
  using Visibility = mozilla::Visibility;

  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;

  explicit nsVideoFrame(nsStyleContext* aContext);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsVideoFrame)

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists) override;

  nsresult AttributeChanged(int32_t aNameSpaceID,
                            nsIAtom* aAttribute,
                            int32_t aModType) override;

  void OnVisibilityChange(Visibility aNewVisibility,
                          const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) override;

  /* get the size of the video's display */
  nsSize GetVideoIntrinsicSize(gfxContext *aRenderingContext);
  nsSize GetIntrinsicRatio() override;
  mozilla::LogicalSize
  ComputeSize(gfxContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags aFlags) override;
  nscoord GetMinISize(gfxContext *aRenderingContext) override;
  nscoord GetPrefISize(gfxContext *aRenderingContext) override;
  void DestroyFrom(nsIFrame* aDestructRoot) override;

  void Reflow(nsPresContext*     aPresContext,
              ReflowOutput&      aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus&    aStatus) override;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsSplittableFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedSizing));
  }

  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilters) override;

  nsIContent* GetPosterImage() { return mPosterImage; }

  // Returns true if we should display the poster. Note that once we show
  // a video frame, the poster will never be displayed again.
  bool ShouldDisplayPoster();

  nsIContent *GetCaptionOverlay() { return mCaptionDiv; }

  nsIContent *GetVideoControls() { return mVideoControls; }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     nsDisplayItem* aItem,
                                     const ContainerLayerParameters& aContainerParameters);

protected:

  // Returns true if we're rendering for a video element. We still create
  // nsVideoFrame to render controls for an audio element.
  bool HasVideoElement();

  // Returns true if there is video data to render. Can return false
  // when we're the frame for an audio element, or we've created a video
  // element for a media which is audio-only.
  bool HasVideoData();

  // Sets the mPosterImage's src attribute to be the video's poster attribute,
  // if we're the frame for a video element. Only call on frames for video
  // elements, not for frames for audio elements.
  void UpdatePosterSource(bool aNotify);

  // Notify the mediaElement that the mCaptionDiv was created.
  void UpdateTextTrack();

  virtual ~nsVideoFrame();

  // Anonymous child which is bound via XBL to the video controls.
  nsCOMPtr<nsIContent> mVideoControls;

  // Anonymous child which is the image element of the poster frame.
  nsCOMPtr<nsIContent> mPosterImage;

  // Anonymous child which is the text track caption display div.
  nsCOMPtr<nsIContent> mCaptionDiv;

};

#endif /* nsVideoFrame_h___ */
