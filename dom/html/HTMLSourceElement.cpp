/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/HTMLSourceElementBinding.h"

#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/MediaSource.h"

#include "nsGkAtoms.h"

#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/Preferences.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Source)

namespace mozilla::dom {

HTMLSourceElement::HTMLSourceElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLSourceElement::~HTMLSourceElement() = default;

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLSourceElement, nsGenericHTMLElement,
                                   mSrcMediaSource)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLSourceElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSourceElement)

bool HTMLSourceElement::MatchesCurrentMedia() {
  if (mMediaList) {
    return mMediaList->Matches(*OwnerDoc());
  }

  // No media specified
  return true;
}

/* static */
bool HTMLSourceElement::WouldMatchMediaForDocument(const nsAString& aMedia,
                                                   const Document* aDocument) {
  if (aMedia.IsEmpty()) {
    return true;
  }

  RefPtr<MediaList> mediaList =
      MediaList::Create(NS_ConvertUTF16toUTF8(aMedia));
  return mediaList->Matches(*aDocument);
}

void HTMLSourceElement::UpdateMediaList(const nsAttrValue* aValue) {
  mMediaList = nullptr;
  if (!aValue) {
    return;
  }

  NS_ConvertUTF16toUTF8 mediaStr(aValue->GetStringValue());
  mMediaList = MediaList::Create(mediaStr);
}

nsresult HTMLSourceElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aMaybeScriptedPrincipal,
                                         bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::srcset) {
    mSrcsetTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
        this, aValue ? aValue->GetStringValue() : EmptyString(),
        aMaybeScriptedPrincipal);
  }
  // If we are associated with a <picture> with a valid <img>, notify it of
  // responsive parameter changes
  Element* parent = nsINode::GetParentElement();
  if (aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::srcset || aName == nsGkAtoms::sizes ||
       aName == nsGkAtoms::media || aName == nsGkAtoms::type) &&
      parent && parent->IsHTMLElement(nsGkAtoms::picture)) {
    if (aName == nsGkAtoms::media) {
      UpdateMediaList(aValue);
    }

    nsString strVal = aValue ? aValue->GetStringValue() : EmptyString();
    // Find all img siblings after this <source> and notify them of the change
    nsCOMPtr<nsIContent> sibling = AsContent();
    while ((sibling = sibling->GetNextSibling())) {
      if (auto* img = HTMLImageElement::FromNode(sibling)) {
        if (aName == nsGkAtoms::srcset) {
          img->PictureSourceSrcsetChanged(this, strVal, aNotify);
        } else if (aName == nsGkAtoms::sizes) {
          img->PictureSourceSizesChanged(this, strVal, aNotify);
        } else if (aName == nsGkAtoms::media || aName == nsGkAtoms::type) {
          img->PictureSourceMediaOrTypeChanged(this, aNotify);
        }
      }
    }
  } else if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::media) {
    UpdateMediaList(aValue);
  } else if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::src) {
    mSrcTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
        this, aValue ? aValue->GetStringValue() : EmptyString(),
        aMaybeScriptedPrincipal);
    mSrcMediaSource = nullptr;
    if (aValue) {
      nsString srcStr = aValue->GetStringValue();
      nsCOMPtr<nsIURI> uri;
      NewURIFromString(srcStr, getter_AddRefs(uri));
      if (uri && IsMediaSourceURI(uri)) {
        NS_GetSourceForMediaSourceURI(uri, getter_AddRefs(mSrcMediaSource));
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

nsresult HTMLSourceElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (auto* media = HTMLMediaElement::FromNode(aParent)) {
    media->NotifyAddedSource();
  }

  return NS_OK;
}

JSObject* HTMLSourceElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLSourceElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
