/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

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
#include "jsapi.h"

#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMProgressEvent.h"
#include "nsIPowerManagerService.h"
#include "MediaError.h"
#include "MediaDecoder.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Video)

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(HTMLVideoElement, HTMLMediaElement)
NS_IMPL_RELEASE_INHERITED(HTMLVideoElement, HTMLMediaElement)

NS_INTERFACE_TABLE_HEAD(HTMLVideoElement)
  NS_HTML_CONTENT_INTERFACES(HTMLMediaElement)
  NS_INTERFACE_TABLE_INHERITED2(HTMLVideoElement, nsIDOMHTMLMediaElement, nsIDOMHTMLVideoElement)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END

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

HTMLVideoElement::HTMLVideoElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : HTMLMediaElement(aNodeInfo)
{
  SetIsDOMBinding();
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

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
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
#ifdef MOZ_OGG
      "video/ogg,"
#endif
      "video/*;q=0.9,"
#ifdef MOZ_OGG
      "application/ogg;q=0.7,"
#endif
      "audio/*;q=0.6,*/*;q=0.5");

  return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                    value,
                                    false);
}

NS_IMPL_URI_ATTR(HTMLVideoElement, Poster, poster)

uint32_t HTMLVideoElement::MozParsedFrames() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on main thread.");
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
  bool hidden = true;

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(OwnerDoc());
  if (domDoc) {
    domDoc->GetHidden(&hidden);
  }

  if (mScreenWakeLock && (mPaused || hidden)) {
    mScreenWakeLock->Unlock();
    mScreenWakeLock = nullptr;
    return;
  }

  if (!mScreenWakeLock && !mPaused && !hidden) {
    nsCOMPtr<nsIPowerManagerService> pmService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
    NS_ENSURE_TRUE_VOID(pmService);

    pmService->NewWakeLock(NS_LITERAL_STRING("screen"),
                           OwnerDoc()->GetWindow(),
                           getter_AddRefs(mScreenWakeLock));
  }
}

} // namespace dom
} // namespace mozilla
