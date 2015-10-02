/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/HTMLSourceElementBinding.h"

#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"
#include "mozilla/dom/MediaSource.h"

#include "nsGkAtoms.h"

#include "nsIMediaList.h"
#include "nsCSSParser.h"
#include "nsHostObjectProtocolHandler.h"

#include "mozilla/Preferences.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Source)

namespace mozilla {
namespace dom {

HTMLSourceElement::HTMLSourceElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLSourceElement::~HTMLSourceElement()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLSourceElement, nsGenericHTMLElement,
                                   mSrcMediaSource)

NS_IMPL_ADDREF_INHERITED(HTMLSourceElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(HTMLSourceElement, nsGenericHTMLElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLSourceElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLSourceElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSourceElement)

NS_IMPL_URI_ATTR(HTMLSourceElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLSourceElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLSourceElement, Srcset, srcset)
NS_IMPL_STRING_ATTR(HTMLSourceElement, Sizes, sizes)
NS_IMPL_STRING_ATTR(HTMLSourceElement, Media, media)

bool
HTMLSourceElement::MatchesCurrentMedia()
{
  if (mMediaList) {
    nsIPresShell* presShell = OwnerDoc()->GetShell();
    nsPresContext* pctx = presShell ? presShell->GetPresContext() : nullptr;
    return pctx && mMediaList->Matches(pctx, nullptr);
  }

  // No media specified
  return true;
}

/* static */ bool
HTMLSourceElement::WouldMatchMediaForDocument(const nsAString& aMedia,
                                              const nsIDocument *aDocument)
{
  if (aMedia.IsEmpty()) {
    return true;
  }

  nsIPresShell* presShell = aDocument->GetShell();
  nsPresContext* pctx = presShell ? presShell->GetPresContext() : nullptr;

  nsCSSParser cssParser;
  nsRefPtr<nsMediaList> mediaList = new nsMediaList();
  cssParser.ParseMediaList(aMedia, nullptr, 0, mediaList, false);

  return pctx && mediaList->Matches(pctx, nullptr);
}

nsresult
HTMLSourceElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
{
  // If we are associated with a <picture> with a valid <img>, notify it of
  // responsive parameter changes
  Element *parent = nsINode::GetParentElement();
  if (aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::srcset ||
       aName == nsGkAtoms::sizes ||
       aName == nsGkAtoms::media ||
       aName == nsGkAtoms::type) &&
      parent && parent->IsHTMLElement(nsGkAtoms::picture)) {
    nsString strVal = aValue ? aValue->GetStringValue() : EmptyString();
    // Find all img siblings after this <source> and notify them of the change
    nsCOMPtr<nsIContent> sibling = AsContent();
    while ( (sibling = sibling->GetNextSibling()) ) {
      if (sibling->IsHTMLElement(nsGkAtoms::img)) {
        HTMLImageElement *img = static_cast<HTMLImageElement*>(sibling.get());
        if (aName == nsGkAtoms::srcset) {
          img->PictureSourceSrcsetChanged(AsContent(), strVal, aNotify);
        } else if (aName == nsGkAtoms::sizes) {
          img->PictureSourceSizesChanged(AsContent(), strVal, aNotify);
        } else if (aName == nsGkAtoms::media ||
                   aName == nsGkAtoms::type) {
          img->PictureSourceMediaOrTypeChanged(AsContent(), aNotify);
        }
      }
    }

  } else if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::media) {
    mMediaList = nullptr;
    if (aValue) {
      nsString mediaStr = aValue->GetStringValue();
      if (!mediaStr.IsEmpty()) {
        nsCSSParser cssParser;
        mMediaList = new nsMediaList();
        cssParser.ParseMediaList(mediaStr, nullptr, 0, mMediaList, false);
      }
    }
  } else if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::src) {
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

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}

void
HTMLSourceElement::GetItemValueText(DOMString& aValue)
{
  GetSrc(aValue);
}

void
HTMLSourceElement::SetItemValueText(const nsAString& aValue)
{
  SetSrc(aValue);
}

nsresult
HTMLSourceElement::BindToTree(nsIDocument *aDocument,
                              nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument,
                                                 aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aParent && aParent->IsNodeOfType(nsINode::eMEDIA)) {
    HTMLMediaElement* media = static_cast<HTMLMediaElement*>(aParent);
    media->NotifyAddedSource();
  } else if (aParent && aParent->IsHTMLElement(nsGkAtoms::picture)) {
    // Find any img siblings after this <source> and notify them
    nsCOMPtr<nsIContent> sibling = AsContent();
    while ( (sibling = sibling->GetNextSibling()) ) {
      if (sibling->IsHTMLElement(nsGkAtoms::img)) {
        HTMLImageElement *img = static_cast<HTMLImageElement*>(sibling.get());
        img->PictureSourceAdded(AsContent());
      }
    }
  }

  return NS_OK;
}

JSObject*
HTMLSourceElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLSourceElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
