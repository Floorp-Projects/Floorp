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

#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/Preferences.h"

#include "nsGkAtoms.h"
#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"

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

bool HTMLSourceElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (StaticPrefs::dom_picture_source_dimension_attributes_enabled() &&
      aNamespaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height)) {
    return aResult.ParseHTMLDimension(aValue);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
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
  if (aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::srcset || aName == nsGkAtoms::sizes ||
       aName == nsGkAtoms::media || aName == nsGkAtoms::type) &&
      IsInPicture()) {
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
  } else if (StaticPrefs::dom_picture_source_dimension_attributes_enabled() &&
             aNameSpaceID == kNameSpaceID_None &&
             IsAttributeMappedToImages(aName) && IsInPicture()) {
    BuildMappedAttributesForImage();

    nsCOMPtr<nsIContent> sibling = AsContent();
    while ((sibling = sibling->GetNextSibling())) {
      if (auto* img = HTMLImageElement::FromNode(sibling)) {
        img->PictureSourceDimensionChanged(this, aNotify);
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

  if (aParent.IsHTMLElement(nsGkAtoms::picture)) {
    BuildMappedAttributesForImage();
  } else {
    mMappedAttributesForImage = nullptr;
  }

  return NS_OK;
}

void HTMLSourceElement::UnbindFromTree(bool aNullParent) {
  mMappedAttributesForImage = nullptr;
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

JSObject* HTMLSourceElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLSourceElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLSourceElement::BuildMappedAttributesForImage() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::dom_picture_source_dimension_attributes_enabled()) {
    return;
  }

  mMappedAttributesForImage = nullptr;

  Document* document = GetComposedDoc();
  nsHTMLStyleSheet* sheet =
      document ? document->GetAttributeStyleSheet() : nullptr;
  if (!sheet) {
    return;
  }

  const nsAttrValue* width = mAttrs.GetAttr(nsGkAtoms::width);
  const nsAttrValue* height = mAttrs.GetAttr(nsGkAtoms::height);
  if (!width && !height) {
    return;
  }

  const size_t count = (width ? 1 : 0) + (height ? 1 : 0);
  RefPtr<nsMappedAttributes> modifiableMapped(new (count) nsMappedAttributes(
      sheet, nsGenericHTMLElement::MapPictureSourceSizeAttributesInto));
  MOZ_ASSERT(modifiableMapped);

  auto maybeSetAttr = [&](nsAtom* aName, const nsAttrValue* aValue) {
    if (!aValue) {
      return;
    }
    nsAttrValue val(*aValue);
    bool oldValueSet = false;
    modifiableMapped->SetAndSwapAttr(aName, val, &oldValueSet);
  };
  maybeSetAttr(nsGkAtoms::width, width);
  maybeSetAttr(nsGkAtoms::height, height);

  RefPtr<nsMappedAttributes> newAttrs =
      sheet->UniqueMappedAttributes(modifiableMapped);
  NS_ENSURE_TRUE_VOID(newAttrs);

  if (newAttrs != modifiableMapped) {
    // Reset the stylesheet of modifiableMapped so that it doesn't
    // spend time trying to remove itself from the hash.  There is no
    // risk that modifiableMapped is in the hash since we created
    // it ourselves and it didn't come from the stylesheet (in which
    // case it would not have been modifiable).
    modifiableMapped->DropStyleSheetReference();
  }

  mMappedAttributesForImage = std::move(newAttrs);
}

}  // namespace mozilla::dom
