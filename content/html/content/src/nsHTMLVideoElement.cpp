/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIDOMHTMLVideoElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsHTMLVideoElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsXPCOMStrings.h"
#include "prlock.h"
#include "nsThreadUtils.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMProgressEvent.h"
#include "nsMediaError.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_NS_NEW_HTML_ELEMENT(Video)

NS_IMPL_ADDREF_INHERITED(nsHTMLVideoElement, nsHTMLMediaElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLVideoElement, nsHTMLMediaElement)

DOMCI_NODE_DATA(HTMLVideoElement, nsHTMLVideoElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLVideoElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLVideoElement, nsIDOMHTMLMediaElement, nsIDOMHTMLVideoElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLVideoElement,
                                               nsHTMLMediaElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLVideoElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLVideoElement)

// nsIDOMHTMLVideoElement
NS_IMPL_INT_ATTR(nsHTMLVideoElement, Width, width)
NS_IMPL_INT_ATTR(nsHTMLVideoElement, Height, height)

// nsIDOMHTMLVideoElement
/* readonly attribute unsigned long videoWidth; */
NS_IMETHODIMP nsHTMLVideoElement::GetVideoWidth(PRUint32 *aVideoWidth)
{
  *aVideoWidth = mMediaSize.width == -1 ? 0 : mMediaSize.width;
  return NS_OK;
}

/* readonly attribute unsigned long videoHeight; */
NS_IMETHODIMP nsHTMLVideoElement::GetVideoHeight(PRUint32 *aVideoHeight)
{
  *aVideoHeight = mMediaSize.height == -1 ? 0 : mMediaSize.height;
  return NS_OK;
}

nsHTMLVideoElement::nsHTMLVideoElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsHTMLMediaElement(aNodeInfo)
{
}

nsHTMLVideoElement::~nsHTMLVideoElement()
{
}

nsresult nsHTMLVideoElement::GetVideoSize(nsIntSize* size)
{
  if (mMediaSize.width == -1 && mMediaSize.height == -1) {
    return NS_ERROR_FAILURE;
  }

  size->height = mMediaSize.height;
  size->width = mMediaSize.width;
  return NS_OK;
}

bool
nsHTMLVideoElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
   if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height) {
     return aResult.ParseSpecialIntValue(aValue);
   }

   return nsHTMLMediaElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
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
nsHTMLVideoElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { nsnull }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
nsHTMLVideoElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

nsresult nsHTMLVideoElement::SetAcceptHeader(nsIHttpChannel* aChannel)
{
  nsCAutoString value(
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

NS_IMPL_URI_ATTR(nsHTMLVideoElement, Poster, poster)

NS_IMETHODIMP nsHTMLVideoElement::GetMozParsedFrames(PRUint32 *aMozParsedFrames)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  *aMozParsedFrames = mDecoder ? mDecoder->GetFrameStatistics().GetParsedFrames() : 0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLVideoElement::GetMozDecodedFrames(PRUint32 *aMozDecodedFrames)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  *aMozDecodedFrames = mDecoder ? mDecoder->GetFrameStatistics().GetDecodedFrames() : 0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLVideoElement::GetMozPresentedFrames(PRUint32 *aMozPresentedFrames)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  *aMozPresentedFrames = mDecoder ? mDecoder->GetFrameStatistics().GetPresentedFrames() : 0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLVideoElement::GetMozPaintedFrames(PRUint32 *aMozPaintedFrames)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  ImageContainer* container = GetImageContainer();
  *aMozPaintedFrames = container ? container->GetPaintCount() : 0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLVideoElement::GetMozFrameDelay(double *aMozFrameDelay) {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  VideoFrameContainer* container = GetVideoFrameContainer();
  *aMozFrameDelay = container ?  container->GetFrameDelay() : 0;
  return NS_OK;
}


/* readonly attribute bool mozHasAudio */
NS_IMETHODIMP nsHTMLVideoElement::GetMozHasAudio(bool *aHasAudio) {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  *aHasAudio = mHasAudio;
  return NS_OK;
}
