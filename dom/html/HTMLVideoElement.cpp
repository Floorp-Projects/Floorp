/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLSourceElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/HTMLVideoElementBinding.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "prlock.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"

#include "nsITimer.h"

#include "MediaError.h"
#include "MediaDecoder.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "nsPerformance.h"
#include "mozilla/dom/VideoPlaybackQuality.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Video)

namespace mozilla {
namespace dom {

static bool sVideoStatsEnabled;

NS_IMPL_ELEMENT_CLONE(HTMLVideoElement)

HTMLVideoElement::HTMLVideoElement(already_AddRefed<NodeInfo>& aNodeInfo)
  : HTMLMediaElement(aNodeInfo)
{
}

HTMLVideoElement::~HTMLVideoElement()
{
}

nsresult HTMLVideoElement::GetVideoSize(nsIntSize* size)
{
  if (!mMediaInfo.HasVideo()) {
    return NS_ERROR_FAILURE;
  }

  if (mDisableVideo) {
    return NS_ERROR_FAILURE;
  }

  size->height = mMediaInfo.mVideo.mDisplay.height;
  size->width = mMediaInfo.mVideo.mDisplay.width;
  return NS_OK;
}

bool
HTMLVideoElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
   if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height) {
     return aResult.ParseSpecialIntValue(aValue);
   }

   return HTMLMediaElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                           aResult);
}

void
HTMLVideoElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLVideoElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLVideoElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

nsresult HTMLVideoElement::SetAcceptHeader(nsIHttpChannel* aChannel)
{
  nsAutoCString value(
#ifdef MOZ_WEBM
      "video/webm,"
#endif
      "video/ogg,"
      "video/*;q=0.9,"
      "application/ogg;q=0.7,"
      "audio/*;q=0.6,*/*;q=0.5");

  return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                    value,
                                    false);
}

bool
HTMLVideoElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::controls) ||
         HTMLMediaElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

uint32_t HTMLVideoElement::MozParsedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  return mDecoder ? mDecoder->GetFrameStatistics().GetParsedFrames() : 0;
}

uint32_t HTMLVideoElement::MozDecodedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  return mDecoder ? mDecoder->GetFrameStatistics().GetDecodedFrames() : 0;
}

uint32_t HTMLVideoElement::MozPresentedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  return mDecoder ? mDecoder->GetFrameStatistics().GetPresentedFrames() : 0;
}

uint32_t HTMLVideoElement::MozPaintedFrames()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  layers::ImageContainer* container = GetImageContainer();
  return container ? container->GetPaintCount() : 0;
}

double HTMLVideoElement::MozFrameDelay()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  VideoFrameContainer* container = GetVideoFrameContainer();
  return container ?  container->GetFrameDelay() : 0;
}

bool HTMLVideoElement::MozHasAudio() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  return HasAudio();
}

JSObject*
HTMLVideoElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLVideoElementBinding::Wrap(aCx, this, aGivenProto);
}

void
HTMLVideoElement::NotifyOwnerDocumentActivityChanged()
{
  HTMLMediaElement::NotifyOwnerDocumentActivityChanged();
  UpdateScreenWakeLock();
}

already_AddRefed<VideoPlaybackQuality>
HTMLVideoElement::GetVideoPlaybackQuality()
{
  DOMHighResTimeStamp creationTime = 0;
  uint64_t totalFrames = 0;
  uint64_t droppedFrames = 0;
  uint64_t corruptedFrames = 0;

  if (sVideoStatsEnabled) {
    nsPIDOMWindow* window = OwnerDoc()->GetInnerWindow();
    if (window) {
      nsPerformance* perf = window->GetPerformance();
      if (perf) {
        creationTime = perf->GetDOMTiming()->TimeStampToDOMHighRes(TimeStamp::Now());
      }
    }

    if (mDecoder) {
      MediaDecoder::FrameStatistics& stats = mDecoder->GetFrameStatistics();
      totalFrames = stats.GetParsedFrames();
      droppedFrames = stats.GetDroppedFrames();
      corruptedFrames = 0;
    }
  }

  nsRefPtr<VideoPlaybackQuality> playbackQuality =
    new VideoPlaybackQuality(this, creationTime, totalFrames, droppedFrames,
                             corruptedFrames);
  return playbackQuality.forget();
}

void
HTMLVideoElement::WakeLockCreate()
{
  HTMLMediaElement::WakeLockCreate();
  UpdateScreenWakeLock();
}

void
HTMLVideoElement::WakeLockRelease()
{
  UpdateScreenWakeLock();
  HTMLMediaElement::WakeLockRelease();
}

void
HTMLVideoElement::UpdateScreenWakeLock()
{
  bool hidden = OwnerDoc()->Hidden();

  if (mScreenWakeLock && (mPaused || hidden)) {
    ErrorResult rv;
    mScreenWakeLock->Unlock(rv);
    NS_WARN_IF_FALSE(!rv.Failed(), "Failed to unlock the wakelock.");
    mScreenWakeLock = nullptr;
    return;
  }

  if (!mScreenWakeLock && !mPaused && !hidden && HasVideo()) {
    nsRefPtr<power::PowerManagerService> pmService =
      power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE_VOID(pmService);

    ErrorResult rv;
    mScreenWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("screen"),
                                             OwnerDoc()->GetInnerWindow(),
                                             rv);
  }
}

void
HTMLVideoElement::Init()
{
  Preferences::AddBoolVarCache(&sVideoStatsEnabled, "media.video_stats.enabled");
}

} // namespace dom
} // namespace mozilla
