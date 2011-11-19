/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsNetUtil.h"
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

nsIntSize nsHTMLVideoElement::GetVideoSize(nsIntSize aDefaultSize)
{
  return mMediaSize.width == -1 && mMediaSize.height == -1 ? aDefaultSize : mMediaSize;
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

  return FindAttributeDependence(aAttribute, map, ArrayLength(map));
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
  *aMozPaintedFrames = (!mDecoder || !GetImageContainer()) ? 0 : GetImageContainer()->GetPaintCount();
  return NS_OK;
}

NS_IMETHODIMP nsHTMLVideoElement::GetMozFrameDelay(double *aMozFrameDelay) {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  *aMozFrameDelay = mDecoder ? mDecoder->GetFrameDelay() : 0;
  return NS_OK;
}
