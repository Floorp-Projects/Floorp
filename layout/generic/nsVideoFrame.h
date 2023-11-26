/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for the HTML <video> element */

#ifndef nsVideoFrame_h___
#define nsVideoFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIReflowCallback.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

class nsPresContext;
class nsDisplayItem;

class nsVideoFrame : public nsContainerFrame,
                     public nsIReflowCallback,
                     public nsIAnonymousContentCreator {
 public:
  template <typename T>
  using Maybe = mozilla::Maybe<T>;
  using Nothing = mozilla::Nothing;
  using Visibility = mozilla::Visibility;

  nsVideoFrame(ComputedStyle* aStyle, nsPresContext* aPc)
      : nsVideoFrame(aStyle, aPc, kClassID) {}
  nsVideoFrame(ComputedStyle*, nsPresContext*, ClassID);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsVideoFrame)

  void ReflowCallbackCanceled() final { mReflowCallbackPosted = false; }
  bool ReflowFinished() final;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) final;

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) final;

  void OnVisibilityChange(
      Visibility aNewVisibility,
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing()) final;

  /* get the size of the video's display */
  mozilla::IntrinsicSize GetIntrinsicSize() final;
  mozilla::AspectRatio GetIntrinsicRatio() const final;
  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) final;
  nscoord GetMinISize(gfxContext* aRenderingContext) final;
  nscoord GetPrefISize(gfxContext* aRenderingContext) final;
  void Destroy(DestroyContext&) final;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput, nsReflowStatus& aStatus) final;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() final;
#endif

  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) final;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilters) final;

  mozilla::dom::Element* GetPosterImage() const { return mPosterImage; }

  // Returns true if we should display the poster. Note that once we show
  // a video frame, the poster will never be displayed again.
  bool ShouldDisplayPoster() const;

  nsIContent* GetCaptionOverlay() const { return mCaptionDiv; }
  nsIContent* GetVideoControls() const;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  // Returns true if we're rendering for a video element. We create an
  // nsAudioFrame (which is still an nsVideoFrame) to render controls for an
  // audio element.
  bool HasVideoElement() const { return !mIsAudio; }

  // Returns true if there is video data to render. Can return false
  // when we're the frame for an audio element, or we've created a video
  // element for a media which is audio-only.
  bool HasVideoData() const;

  // Get the poster image's size if there is any.
  mozilla::Maybe<nsSize> PosterImageSize() const;

  // Sets the mPosterImage's src attribute to be the video's poster attribute,
  // if we're the frame for a video element. Only call on frames for video
  // elements, not for frames for audio elements.
  void UpdatePosterSource(bool aNotify);

  // Notify the mediaElement that the mCaptionDiv was created.
  void UpdateTextTrack();

  virtual ~nsVideoFrame();

  // Anonymous child which is the image element of the poster frame.
  RefPtr<mozilla::dom::Element> mPosterImage;

  // Anonymous child which is the text track caption display div.
  nsCOMPtr<nsIContent> mCaptionDiv;

  // Some sizes tracked for notification purposes.
  // TODO: Maybe the calling code could be rewritten to use ResizeObserver for
  // this nowadays.
  nsSize mControlsTrackedSize{-1, -1};
  nsSize mCaptionTrackedSize{-1, -1};
  bool mReflowCallbackPosted = false;
  const bool mIsAudio;
};

// NOTE(emilio): This class here only for the purpose of having different
// ClassFlags for <audio> elements. This frame shouldn't contain extra logic, as
// things are set up now, because <audio> can also use video controls etc.
// In the future we might want to rejigger this to be less weird (e.g, an audio
// frame might not need a caption, or text tracks, or what not).
class nsAudioFrame final : public nsVideoFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsAudioFrame)

  nsAudioFrame(ComputedStyle*, nsPresContext*);
  virtual ~nsAudioFrame();
};

#endif /* nsVideoFrame_h___ */
