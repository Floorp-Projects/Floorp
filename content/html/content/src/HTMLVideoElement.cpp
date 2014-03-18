/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLVideoElement.h"
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

#include "nsEventDispatcher.h"
#include "nsIDOMProgressEvent.h"
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

NS_IMPL_ISUPPORTS_INHERITED2(HTMLVideoElement, HTMLMediaElement,
                             nsIDOMHTMLMediaElement, nsIDOMHTMLVideoElement)

NS_IMPL_ELEMENT_CLONE(HTMLVideoElement)

// nsIDOMHTMLVideoElement
NS_IMPL_INT_ATTR(HTMLVideoElement, Width, width)
NS_IMPL_INT_ATTR(HTMLVideoElement, Height, height)

// nsIDOMHTMLVideoElement
/* readonly attribute unsigned long videoWidth; */
NS_IMETHODIMP HTMLVideoElement::GetVideoWidth(uint32_t *aVideoWidth)
{
  *aVideoWidth = VideoWidth();
  return NS_OK;
}

/* readonly attribute unsigned long videoHeight; */
NS_IMETHODIMP HTMLVideoElement::GetVideoHeight(uint32_t *aVideoHeight)
{
  *aVideoHeight = VideoHeight();
  return NS_OK;
}

HTMLVideoElement::HTMLVideoElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : HTMLMediaElement(aNodeInfo)
{
}

HTMLVideoElement::~HTMLVideoElement()
{
}

nsresult HTMLVideoElement::GetVideoSize(nsIntSize* size)
{
  if (mMediaSize.width == -1 && mMediaSize.height == -1) {
    return NS_ERROR_FAILURE;
  }

  size->height = mMediaSize.height;
  size->width = mMediaSize.width;
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

NS_IMPL_URI_ATTR(HTMLVideoElement, Poster, poster)

uint32_t HTMLVideoElement::MozParsedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  return mDecoder ? mDecoder->GetFrameStatistics().GetParsedFrames() : 0;
}

NS_IMETHODIMP HTMLVideoElement::GetMozParsedFrames(uint32_t *aMozParsedFrames)
{
  *aMozParsedFrames = MozParsedFrames();
  return NS_OK;
}

uint32_t HTMLVideoElement::MozDecodedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  return mDecoder ? mDecoder->GetFrameStatistics().GetDecodedFrames() : 0;
}

NS_IMETHODIMP HTMLVideoElement::GetMozDecodedFrames(uint32_t *aMozDecodedFrames)
{
  *aMozDecodedFrames = MozDecodedFrames();
  return NS_OK;
}

uint32_t HTMLVideoElement::MozPresentedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  if (!sVideoStatsEnabled) {
    return 0;
  }
  return mDecoder ? mDecoder->GetFrameStatistics().GetPresentedFrames() : 0;
}

NS_IMETHODIMP HTMLVideoElement::GetMozPresentedFrames(uint32_t *aMozPresentedFrames)
{
  *aMozPresentedFrames = MozPresentedFrames();
  return NS_OK;
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

NS_IMETHODIMP HTMLVideoElement::GetMozPaintedFrames(uint32_t *aMozPaintedFrames)
{
  *aMozPaintedFrames = MozPaintedFrames();
  return NS_OK;
}

double HTMLVideoElement::MozFrameDelay()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  VideoFrameContainer* container = GetVideoFrameContainer();
  return container ?  container->GetFrameDelay() : 0;
}

NS_IMETHODIMP HTMLVideoElement::GetMozFrameDelay(double *aMozFrameDelay) {
  *aMozFrameDelay = MozFrameDelay();
  return NS_OK;
}

/* readonly attribute bool mozHasAudio */
bool HTMLVideoElement::MozHasAudio() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
  return mHasAudio;
}

NS_IMETHODIMP HTMLVideoElement::GetMozHasAudio(bool *aHasAudio) {
  *aHasAudio = MozHasAudio();
  return NS_OK;
}

JSObject*
HTMLVideoElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLVideoElementBinding::Wrap(aCx, aScope, this);
}

void
HTMLVideoElement::NotifyOwnerDocumentActivityChanged()
{
  HTMLMediaElement::NotifyOwnerDocumentActivityChanged();
  WakeLockUpdate();
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
      droppedFrames = totalFrames - stats.GetPresentedFrames();
      corruptedFrames = totalFrames - stats.GetDecodedFrames();
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
  WakeLockUpdate();
}

void
HTMLVideoElement::WakeLockRelease()
{
  WakeLockUpdate();
}

void
HTMLVideoElement::WakeLockUpdate()
{
  bool hidden = OwnerDoc()->Hidden();

  if (mScreenWakeLock && (mPaused || hidden)) {
    ErrorResult rv;
    mScreenWakeLock->Unlock(rv);
    NS_WARN_IF_FALSE(!rv.Failed(), "Failed to unlock the wakelock.");
    mScreenWakeLock = nullptr;
    return;
  }

  if (!mScreenWakeLock && !mPaused && !hidden) {
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
