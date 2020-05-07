/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLVideoElement.h"

#include "mozilla/AsyncEventDispatcher.h"
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
#include "VideoOutput.h"

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
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <limits>

nsGenericHTMLElement* NS_NewHTMLVideoElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  auto* nim = nodeInfo->NodeInfoManager();
  mozilla::dom::HTMLVideoElement* element =
      new (nim) mozilla::dom::HTMLVideoElement(nodeInfo.forget());
  element->Init();
  return element;
}

namespace mozilla {
namespace dom {

class HTMLVideoElement::SecondaryVideoOutput : public VideoOutput {
  // Main thread only.
  WeakPtr<HTMLMediaElement> mElement;

 public:
  SecondaryVideoOutput(HTMLMediaElement* aElement,
                       VideoFrameContainer* aContainer,
                       AbstractThread* aMainThread)
      : VideoOutput(aContainer, aMainThread), mElement(aElement) {}

  void Forget() {
    MOZ_ASSERT(NS_IsMainThread());
    mElement = nullptr;
  }

  void NotifyDirectListenerInstalled(InstallationResult aResult) override {
    if (aResult == InstallationResult::SUCCESS) {
      mMainThread->Dispatch(NS_NewRunnableFunction(
          "HTMLMediaElement::OnSecondaryVideoContainerInstalled",
          [self = RefPtr<SecondaryVideoOutput>(this), this] {
            if (mElement) {
              mElement->OnSecondaryVideoContainerInstalled(
                  mVideoFrameContainer);
            }
          }));
    }
  }
};

nsresult HTMLVideoElement::Clone(mozilla::dom::NodeInfo* aNodeInfo,
                                 nsINode** aResult) const {
  *aResult = nullptr;
  RefPtr<mozilla::dom::NodeInfo> ni(aNodeInfo);
  auto* nim = ni->NodeInfoManager();
  HTMLVideoElement* it = new (nim) HTMLVideoElement(ni.forget());
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

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLVideoElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVisualCloneTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVisualCloneTargetPromise)
  if (tmp->mSecondaryVideoOutput) {
    MOZ_DIAGNOSTIC_ASSERT(tmp->mSelectedVideoStreamTrack);
    tmp->mSelectedVideoStreamTrack->RemoveVideoOutput(
        tmp->mSecondaryVideoOutput);
    tmp->mSecondaryVideoOutput->Forget();
    tmp->mSecondaryVideoOutput = nullptr;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVisualCloneSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(HTMLMediaElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLVideoElement,
                                                  HTMLMediaElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVisualCloneTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVisualCloneTargetPromise)
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
    return aResult.ParseHTMLDimension(aValue);
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

void HTMLVideoElement::UnbindFromTree(bool aNullParent) {
  if (mVisualCloneSource) {
    mVisualCloneSource->EndCloningVisually();
  } else if (mVisualCloneTarget) {
    RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
        this, NS_LITERAL_STRING("MozStopPictureInPicture"), CanBubble::eNo,
        ChromeOnlyDispatch::eYes);
    asyncDispatcher->RunDOMEventWhenSafe();

    EndCloningVisually();
  }

  HTMLMediaElement::UnbindFromTree(aNullParent);
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

bool HTMLVideoElement::IsInteractiveHTMLContent() const {
  return HasAttr(kNameSpaceID_None, nsGkAtoms::controls) ||
         HTMLMediaElement::IsInteractiveHTMLContent();
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
      }
    }
  }

  RefPtr<VideoPlaybackQuality> playbackQuality =
      new VideoPlaybackQuality(this, creationTime, totalFrames, droppedFrames);
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
  // Only request wake lock for video with audio or video from media stream,
  // because non-stream video without audio is often used as a background image.
  //
  // Some web conferencing sites route audio outside the video element, and
  // would not be detected unless we check for media stream, so do that below.
  //
  // Media streams generally aren't used as background images, though if they
  // were we'd get false positives. If this is an issue, we could check for
  // media stream AND document has audio playing (but that was tricky to do).
  return HasVideo() && (mSrcStream || HasAudio());
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
    RefPtr<HTMLVideoElement> aVisualCloneTarget,
    RefPtr<Promise> aVisualCloneTargetPromise) {
  MOZ_DIAGNOSTIC_ASSERT(
      !aVisualCloneTarget || aVisualCloneTarget->IsInComposedDoc(),
      "Can't set the clone target to a disconnected video "
      "element.");
  MOZ_DIAGNOSTIC_ASSERT(!mVisualCloneSource,
                        "Can't clone a video element that is already a clone.");
  if (!aVisualCloneTarget ||
      (aVisualCloneTarget->IsInComposedDoc() && !mVisualCloneSource)) {
    mVisualCloneTarget = std::move(aVisualCloneTarget);
    mVisualCloneTargetPromise = std::move(aVisualCloneTargetPromise);
    return true;
  }
  return false;
}

bool HTMLVideoElement::SetVisualCloneSource(
    RefPtr<HTMLVideoElement> aVisualCloneSource) {
  MOZ_DIAGNOSTIC_ASSERT(
      !aVisualCloneSource || aVisualCloneSource->IsInComposedDoc(),
      "Can't set the clone source to a disconnected video "
      "element.");
  MOZ_DIAGNOSTIC_ASSERT(!mVisualCloneTarget,
                        "Can't clone a video element that is already a "
                        "clone.");
  if (!aVisualCloneSource ||
      (aVisualCloneSource->IsInComposedDoc() && !mVisualCloneTarget)) {
    mVisualCloneSource = std::move(aVisualCloneSource);
    return true;
  }
  return false;
}

/* static */
bool HTMLVideoElement::IsVideoStatsEnabled() {
  return StaticPrefs::media_video_stats_enabled();
}

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

already_AddRefed<Promise> HTMLVideoElement::CloneElementVisually(
    HTMLVideoElement& aTargetVideo, ErrorResult& aRv) {
  MOZ_ASSERT(IsInComposedDoc(),
             "Can't clone a video that's not bound to a DOM tree.");
  MOZ_ASSERT(aTargetVideo.IsInComposedDoc(),
             "Can't clone to a video that's not bound to a DOM tree.");
  if (!IsInComposedDoc() || !aTargetVideo.IsInComposedDoc()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsPIDOMWindowInner* win = OwnerDoc()->GetInnerWindow();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(win->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Do we already have a visual clone target? If so, shut it down.
  if (mVisualCloneTarget) {
    EndCloningVisually();
  }

  // If there's a poster set on the target video, clear it, otherwise
  // it'll display over top of the cloned frames.
  aTargetVideo.UnsetHTMLAttr(nsGkAtoms::poster, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!SetVisualCloneTarget(&aTargetVideo, promise)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!aTargetVideo.SetVisualCloneSource(this)) {
    mVisualCloneTarget = nullptr;
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  aTargetVideo.SetMediaInfo(mMediaInfo);

  if (IsInComposedDoc() && !StaticPrefs::media_cloneElementVisually_testing()) {
    NotifyUAWidgetSetupOrChange();
  }

  MaybeBeginCloningVisually();

  return promise.forget();
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
    VideoFrameContainer* container =
        mVisualCloneTarget->GetVideoFrameContainer();
    if (container) {
      mDecoder->SetSecondaryVideoContainer(container);
    }
    UpdateMediaControlAfterPictureInPictureModeChanged();
  } else if (mSrcStream) {
    VideoFrameContainer* container =
        mVisualCloneTarget->GetVideoFrameContainer();
    if (container && mSelectedVideoStreamTrack) {
      MOZ_DIAGNOSTIC_ASSERT(!mSecondaryVideoOutput);
      mSecondaryVideoOutput = MakeRefPtr<SecondaryVideoOutput>(
          this, container, mAbstractMainThread);
      mSelectedVideoStreamTrack->AddVideoOutput(mSecondaryVideoOutput);
    }
    UpdateMediaControlAfterPictureInPictureModeChanged();
  }
}

void HTMLVideoElement::EndCloningVisually() {
  MOZ_ASSERT(mVisualCloneTarget);

  if (mDecoder) {
    mDecoder->SetSecondaryVideoContainer(nullptr);
  } else if (mSrcStream) {
    if (mSecondaryVideoOutput &&
        mVisualCloneTarget->mSelectedVideoStreamTrack) {
      mVisualCloneTarget->mSelectedVideoStreamTrack->RemoveVideoOutput(
          mSecondaryVideoOutput);
      mSecondaryVideoOutput->Forget();
      mSecondaryVideoOutput = nullptr;
    }
  }

  Unused << mVisualCloneTarget->SetVisualCloneSource(nullptr);
  Unused << SetVisualCloneTarget(nullptr);

  UpdateMediaControlAfterPictureInPictureModeChanged();

  if (IsInComposedDoc() && !StaticPrefs::media_cloneElementVisually_testing()) {
    NotifyUAWidgetSetupOrChange();
  }
}

void HTMLVideoElement::OnSecondaryVideoContainerInstalled(
    const RefPtr<VideoFrameContainer>& aSecondaryContainer) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT_IF(mVisualCloneTargetPromise, mVisualCloneTarget);
  if (!mVisualCloneTargetPromise) {
    // Clone target was unset.
    return;
  }

  VideoFrameContainer* container = mVisualCloneTarget->GetVideoFrameContainer();
  if (NS_WARN_IF(container != aSecondaryContainer)) {
    // Not the right container.
    return;
  }

  mMainThreadEventTarget->Dispatch(NewRunnableMethod(
      "Promise::MaybeResolveWithUndefined", mVisualCloneTargetPromise,
      &Promise::MaybeResolveWithUndefined));
  mVisualCloneTargetPromise = nullptr;
}

}  // namespace dom
}  // namespace mozilla
