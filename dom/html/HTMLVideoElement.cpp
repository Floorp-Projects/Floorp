/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/HTMLVideoElementBinding.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "prlock.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"
#include "VideoFrameContainer.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"

#include "nsITimer.h"

#include "FrameStatistics.h"
#include "MediaError.h"
#include "MediaDecoder.h"
#include "MediaDecoderStateMachine.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/dom/VideoPlaybackQuality.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <limits>

nsGenericHTMLElement* NS_NewHTMLVideoElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser) {
  mozilla::dom::HTMLVideoElement* element =
      new mozilla::dom::HTMLVideoElement(std::move(aNodeInfo));
  element->Init();
  return element;
}

namespace mozilla {
namespace dom {

static bool sVideoStatsEnabled;
static bool sCloneElementVisuallyTesting;

nsresult HTMLVideoElement::Clone(mozilla::dom::NodeInfo* aNodeInfo,
                                 nsINode** aResult) const {
  *aResult = nullptr;
  RefPtr<mozilla::dom::NodeInfo> ni(aNodeInfo);
  HTMLVideoElement* it = new HTMLVideoElement(ni.forget());
  it->Init();
  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = const_cast<HTMLVideoElement*>(this)->CopyInnerTo(it);
  if (NS_SUCCEEDED(rv)) {
    kungFuDeathGrip.swap(*aResult);
  }
  return rv;
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLVideoElement,
                                               HTMLMediaElement)

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLVideoElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLVideoElement,
                                                HTMLMediaElement)
  if (tmp->mVisualCloneTarget) {
    tmp->EndCloningVisually();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVisualCloneTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVisualCloneSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLVideoElement,
                                                  HTMLMediaElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVisualCloneTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVisualCloneSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

HTMLVideoElement::HTMLVideoElement(already_AddRefed<NodeInfo>&& aNodeInfo)
    : HTMLMediaElement(std::move(aNodeInfo)), mIsOrientationLocked(false) {
  DecoderDoctorLogger::LogConstruction(this);
}

HTMLVideoElement::~HTMLVideoElement() {
  DecoderDoctorLogger::LogDestruction(this);
}

void HTMLVideoElement::UpdateMediaSize(const nsIntSize& aSize) {
  HTMLMediaElement::UpdateMediaSize(aSize);
  // If we have a clone target, we should update its size as well.
  if (mVisualCloneTarget) {
    Maybe<nsIntSize> newSize = Some(aSize);
    mVisualCloneTarget->Invalidate(true, newSize, true);
  }
}

nsresult HTMLVideoElement::GetVideoSize(nsIntSize* size) {
  if (!mMediaInfo.HasVideo()) {
    return NS_ERROR_FAILURE;
  }

  if (mDisableVideo) {
    return NS_ERROR_FAILURE;
  }

  switch (mMediaInfo.mVideo.mRotation) {
    case VideoInfo::Rotation::kDegree_90:
    case VideoInfo::Rotation::kDegree_270: {
      size->width = mMediaInfo.mVideo.mDisplay.height;
      size->height = mMediaInfo.mVideo.mDisplay.width;
      break;
    }
    case VideoInfo::Rotation::kDegree_0:
    case VideoInfo::Rotation::kDegree_180:
    default: {
      size->height = mMediaInfo.mVideo.mDisplay.height;
      size->width = mMediaInfo.mVideo.mDisplay.width;
      break;
    }
  }
  return NS_OK;
}

void HTMLVideoElement::Invalidate(bool aImageSizeChanged,
                                  Maybe<nsIntSize>& aNewIntrinsicSize,
                                  bool aForceInvalidate) {
  HTMLMediaElement::Invalidate(aImageSizeChanged, aNewIntrinsicSize,
                               aForceInvalidate);
  if (mVisualCloneTarget) {
    VideoFrameContainer* container =
        mVisualCloneTarget->GetVideoFrameContainer();
    if (container) {
      container->Invalidate();
    }
  }
}

bool HTMLVideoElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      nsAttrValue& aResult) {
  if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height) {
    return aResult.ParseSpecialIntValue(aValue);
  }

  return HTMLMediaElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                          aMaybeScriptedPrincipal, aResult);
}

void HTMLVideoElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLVideoElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
      {nsGkAtoms::width}, {nsGkAtoms::height}, {nullptr}};

  static const MappedAttributeEntry* const map[] = {attributes,
                                                    sCommonAttributeMap};

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLVideoElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

void HTMLVideoElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  if (mVisualCloneSource) {
    mVisualCloneSource->EndCloningVisually();
  } else if (mVisualCloneTarget) {
    EndCloningVisually();
  }

  HTMLMediaElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult HTMLVideoElement::SetAcceptHeader(nsIHttpChannel* aChannel) {
  nsAutoCString value(
      "video/webm,"
      "video/ogg,"
      "video/*;q=0.9,"
      "application/ogg;q=0.7,"
      "audio/*;q=0.6,*/*;q=0.5");

  return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"), value, false);
}

bool HTMLVideoElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const {
  return HasAttr(kNameSpaceID_None, nsGkAtoms::controls) ||
         HTMLMediaElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

uint32_t HTMLVideoElement::MozParsedFrames() const {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!IsVideoStatsEnabled()) {
    return 0;
  }

  if (nsContentUtils::ShouldResistFingerprinting(OwnerDoc())) {
    return nsRFPService::GetSpoofedTotalFrames(TotalPlayTime());
  }

  return mDecoder ? mDecoder->GetFrameStatistics().GetParsedFrames() : 0;
}

uint32_t HTMLVideoElement::MozDecodedFrames() const {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!IsVideoStatsEnabled()) {
    return 0;
  }

  if (nsContentUtils::ShouldResistFingerprinting(OwnerDoc())) {
    return nsRFPService::GetSpoofedTotalFrames(TotalPlayTime());
  }

  return mDecoder ? mDecoder->GetFrameStatistics().GetDecodedFrames() : 0;
}

uint32_t HTMLVideoElement::MozPresentedFrames() const {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!IsVideoStatsEnabled()) {
    return 0;
  }

  if (nsContentUtils::ShouldResistFingerprinting(OwnerDoc())) {
    return nsRFPService::GetSpoofedPresentedFrames(TotalPlayTime(),
                                                   VideoWidth(), VideoHeight());
  }

  return mDecoder ? mDecoder->GetFrameStatistics().GetPresentedFrames() : 0;
}

uint32_t HTMLVideoElement::MozPaintedFrames() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!IsVideoStatsEnabled()) {
    return 0;
  }

  if (nsContentUtils::ShouldResistFingerprinting(OwnerDoc())) {
    return nsRFPService::GetSpoofedPresentedFrames(TotalPlayTime(),
                                                   VideoWidth(), VideoHeight());
  }

  layers::ImageContainer* container = GetImageContainer();
  return container ? container->GetPaintCount() : 0;
}

double HTMLVideoElement::MozFrameDelay() {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");

  if (!IsVideoStatsEnabled() ||
      nsContentUtils::ShouldResistFingerprinting(OwnerDoc())) {
    return 0.0;
  }

  VideoFrameContainer* container = GetVideoFrameContainer();
  // Hide negative delays. Frame timing tweaks in the compositor (e.g.
  // adding a bias value to prevent multiple dropped/duped frames when
  // frame times are aligned with composition times) may produce apparent
  // negative delay, but we shouldn't report that.
  return container ? std::max(0.0, container->GetFrameDelay()) : 0.0;
}

bool HTMLVideoElement::MozHasAudio() const {
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  return HasAudio();
}

JSObject* HTMLVideoElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return HTMLVideoElement_Binding::Wrap(aCx, this, aGivenProto);
}

FrameStatistics* HTMLVideoElement::GetFrameStatistics() {
  return mDecoder ? &(mDecoder->GetFrameStatistics()) : nullptr;
}

already_AddRefed<VideoPlaybackQuality>
HTMLVideoElement::GetVideoPlaybackQuality() {
  DOMHighResTimeStamp creationTime = 0;
  uint32_t totalFrames = 0;
  uint32_t droppedFrames = 0;
  uint32_t corruptedFrames = 0;

  if (IsVideoStatsEnabled()) {
    if (nsPIDOMWindowInner* window = OwnerDoc()->GetInnerWindow()) {
      Performance* perf = window->GetPerformance();
      if (perf) {
        creationTime = perf->Now();
      }
    }

    if (mDecoder) {
      if (nsContentUtils::ShouldResistFingerprinting(OwnerDoc())) {
        totalFrames = nsRFPService::GetSpoofedTotalFrames(TotalPlayTime());
        droppedFrames = nsRFPService::GetSpoofedDroppedFrames(
            TotalPlayTime(), VideoWidth(), VideoHeight());
        corruptedFrames = 0;
      } else {
        FrameStatistics* stats = &mDecoder->GetFrameStatistics();
        if (sizeof(totalFrames) >= sizeof(stats->GetParsedFrames())) {
          totalFrames = stats->GetTotalFrames();
          droppedFrames = stats->GetDroppedFrames();
        } else {
          uint64_t total = stats->GetTotalFrames();
          const auto maxNumber = std::numeric_limits<uint32_t>::max();
          if (total <= maxNumber) {
            totalFrames = uint32_t(total);
            droppedFrames = uint32_t(stats->GetDroppedFrames());
          } else {
            // Too big number(s) -> Resize everything to fit in 32 bits.
            double ratio = double(maxNumber) / double(total);
            totalFrames = maxNumber;  // === total * ratio
            droppedFrames = uint32_t(double(stats->GetDroppedFrames()) * ratio);
          }
        }
        corruptedFrames = 0;
      }
    }
  }

  RefPtr<VideoPlaybackQuality> playbackQuality = new VideoPlaybackQuality(
      this, creationTime, totalFrames, droppedFrames, corruptedFrames);
  return playbackQuality.forget();
}

void HTMLVideoElement::WakeLockRelease() {
  HTMLMediaElement::WakeLockRelease();
  ReleaseVideoWakeLockIfExists();
}

void HTMLVideoElement::UpdateWakeLock() {
  HTMLMediaElement::UpdateWakeLock();
  if (!mPaused) {
    CreateVideoWakeLockIfNeeded();
  } else {
    ReleaseVideoWakeLockIfExists();
  }
}

bool HTMLVideoElement::ShouldCreateVideoWakeLock() const {
  // Make sure we only request wake lock for video with audio track, because
  // video without audio track is often used as background image which seems no
  // need to hold a wakelock.
  return HasVideo() && HasAudio();
}

void HTMLVideoElement::CreateVideoWakeLockIfNeeded() {
  if (!mScreenWakeLock && ShouldCreateVideoWakeLock()) {
    RefPtr<power::PowerManagerService> pmService =
        power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE_VOID(pmService);

    ErrorResult rv;
    mScreenWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("video-playing"),
                                             OwnerDoc()->GetInnerWindow(), rv);
  }
}

void HTMLVideoElement::ReleaseVideoWakeLockIfExists() {
  if (mScreenWakeLock) {
    ErrorResult rv;
    mScreenWakeLock->Unlock(rv);
    rv.SuppressException();
    mScreenWakeLock = nullptr;
    return;
  }
}

bool HTMLVideoElement::SetVisualCloneTarget(
    HTMLVideoElement* aVisualCloneTarget) {
  MOZ_DIAGNOSTIC_ASSERT(
      !aVisualCloneTarget || !aVisualCloneTarget->mUnboundFromTree,
      "Can't set the clone target to a disconnected video "
      "element.");
  MOZ_DIAGNOSTIC_ASSERT(!mVisualCloneSource,
                        "Can't clone a video element that is already a clone.");
  if (!aVisualCloneTarget ||
      (!aVisualCloneTarget->mUnboundFromTree && !mVisualCloneSource)) {
    mVisualCloneTarget = aVisualCloneTarget;
    return true;
  }
  return false;
}

bool HTMLVideoElement::SetVisualCloneSource(
    HTMLVideoElement* aVisualCloneSource) {
  MOZ_DIAGNOSTIC_ASSERT(
      !aVisualCloneSource || !aVisualCloneSource->mUnboundFromTree,
      "Can't set the clone source to a disconnected video "
      "element.");
  MOZ_DIAGNOSTIC_ASSERT(!mVisualCloneTarget,
                        "Can't clone a video element that is already a "
                        "clone.");
  if (!aVisualCloneSource ||
      (!aVisualCloneSource->mUnboundFromTree && !mVisualCloneTarget)) {
    mVisualCloneSource = aVisualCloneSource;
    return true;
  }
  return false;
}

/* static */
void HTMLVideoElement::InitStatics() {
  Preferences::AddBoolVarCache(&sVideoStatsEnabled,
                               "media.video_stats.enabled");
  Preferences::AddBoolVarCache(&sCloneElementVisuallyTesting,
                               "media.cloneElementVisually.testing");
}

/* static */
bool HTMLVideoElement::IsVideoStatsEnabled() { return sVideoStatsEnabled; }

double HTMLVideoElement::TotalPlayTime() const {
  double total = 0.0;

  if (mPlayed) {
    uint32_t timeRangeCount = mPlayed->Length();

    for (uint32_t i = 0; i < timeRangeCount; i++) {
      double begin = mPlayed->Start(i);
      double end = mPlayed->End(i);
      total += end - begin;
    }

    if (mCurrentPlayRangeStart != -1.0) {
      double now = CurrentTime();
      if (mCurrentPlayRangeStart != now) {
        total += now - mCurrentPlayRangeStart;
      }
    }
  }

  return total;
}

void HTMLVideoElement::CloneElementVisually(HTMLVideoElement& aTargetVideo,
                                            ErrorResult& rv) {
  MOZ_ASSERT(!mUnboundFromTree,
             "Can't clone a video that's not bound to a DOM tree.");
  MOZ_ASSERT(!aTargetVideo.mUnboundFromTree,
             "Can't clone to a video that's not bound to a DOM tree.");
  if (mUnboundFromTree || aTargetVideo.mUnboundFromTree) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  // Do we already have a visual clone target? If so, shut it down.
  if (mVisualCloneTarget) {
    EndCloningVisually();
  }

  // If there's a poster set on the target video, clear it, otherwise
  // it'll display over top of the cloned frames.
  aTargetVideo.UnsetHTMLAttr(nsGkAtoms::poster, rv);
  if (rv.Failed()) {
    return;
  }

  if (!SetVisualCloneTarget(&aTargetVideo)) {
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!aTargetVideo.SetVisualCloneSource(this)) {
    mVisualCloneTarget = nullptr;
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aTargetVideo.SetMediaInfo(mMediaInfo);

  if (IsInComposedDoc() && !sCloneElementVisuallyTesting) {
    NotifyUAWidgetSetupOrChange();
  }

  MaybeBeginCloningVisually();
}

void HTMLVideoElement::StopCloningElementVisually() {
  if (mVisualCloneTarget) {
    EndCloningVisually();
  }
}

void HTMLVideoElement::MaybeBeginCloningVisually() {
  if (!mVisualCloneTarget) {
    return;
  }

  if (mDecoder) {
    MediaDecoderStateMachine* mdsm = mDecoder->GetStateMachine();
    VideoFrameContainer* container =
        mVisualCloneTarget->GetVideoFrameContainer();
    if (mdsm && container) {
      mdsm->SetSecondaryVideoContainer(container);
      mDecoder->SetCloningVisually(true);
    }
  } else if (mSrcStream) {
    VideoFrameContainer* container =
        mVisualCloneTarget->GetVideoFrameContainer();
    if (container && mSelectedVideoStreamTrack) {
      mSelectedVideoStreamTrack->AddVideoOutput(container);
    }
  }
}

void HTMLVideoElement::EndCloningVisually() {
  MOZ_ASSERT(mVisualCloneTarget);

  if (mDecoder) {
    MediaDecoderStateMachine* mdsm = mDecoder->GetStateMachine();
    if (mdsm) {
      mdsm->SetSecondaryVideoContainer(nullptr);
      mDecoder->SetCloningVisually(false);
    }
  } else if (mSrcStream) {
    VideoFrameContainer* container =
        mVisualCloneTarget->GetVideoFrameContainer();
    if (container && mVisualCloneTarget->mSelectedVideoStreamTrack) {
      mVisualCloneTarget->mSelectedVideoStreamTrack->RemoveVideoOutput(
          container);
    }
  }

  Unused << mVisualCloneTarget->SetVisualCloneSource(nullptr);
  Unused << SetVisualCloneTarget(nullptr);

  if (IsInComposedDoc() && !sCloneElementVisuallyTesting) {
    NotifyUAWidgetSetupOrChange();
  }
}

void HTMLVideoElement::TogglePictureInPicture(ErrorResult& error) {
  // The MozTogglePictureInPicture event is listen for via the
  // PictureInPictureChild actor, which is responsible for opening the new
  // window and starting the visual clone.
  nsresult rv = DispatchEvent(NS_LITERAL_STRING("MozTogglePictureInPicture"));
  if (NS_FAILED(rv)) {
    error.Throw(rv);
  }
}

}  // namespace dom
}  // namespace mozilla
