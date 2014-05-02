/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/HTMLSourceElementBinding.h"

#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"

#include "nsGkAtoms.h"

#include "nsIMediaList.h"
#include "nsCSSParser.h"

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

NS_IMPL_ISUPPORTS_INHERITED(HTMLSourceElement, nsGenericHTMLElement,
                            nsIDOMHTMLSourceElement)

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

nsresult
HTMLSourceElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
{
  // If we are associated with a <picture> with a valid <img>, notify it of
  // responsive parameter changes
  nsINode *parent = nsINode::GetParentNode();
  if (aNameSpaceID == kNameSpaceID_None &&
      (aName == nsGkAtoms::srcset || aName == nsGkAtoms::sizes) &&
      parent && parent->Tag() == nsGkAtoms::picture && MatchesCurrentMedia()) {

    nsString strVal = aValue ? aValue->GetStringValue() : EmptyString();
    // Find all img siblings after this <source> and notify them of the change
    nsCOMPtr<nsINode> sibling = AsContent();
    while ( (sibling = sibling->GetNextSibling()) ) {
      if (sibling->Tag() == nsGkAtoms::img) {
        HTMLImageElement *img = static_cast<HTMLImageElement*>(sibling.get());
        if (aName == nsGkAtoms::srcset) {
          img->PictureSourceSrcsetChanged(AsContent(), strVal, aNotify);
        } else if (aName == nsGkAtoms::sizes) {
          img->PictureSourceSizesChanged(AsContent(), strVal, aNotify);
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
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}

void
HTMLSourceElement::GetItemValueText(nsAString& aValue)
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
  } else if (aParent && aParent->Tag() == nsGkAtoms::picture) {
    // Find any img siblings after this <source> and notify them
    nsCOMPtr<nsINode> sibling = AsContent();
    while ( (sibling = sibling->GetNextSibling()) ) {
      if (sibling->Tag() == nsGkAtoms::img) {
        HTMLImageElement *img = static_cast<HTMLImageElement*>(sibling.get());
        img->PictureSourceAdded(AsContent());
      }
    }
  }

  return NS_OK;
}

void
HTMLSourceElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsINode *parent = nsINode::GetParentNode();
  if (parent && parent->Tag() == nsGkAtoms::picture) {
    // Find all img siblings after this <source> and notify them of our demise
    nsCOMPtr<nsINode> sibling = AsContent();
    while ( (sibling = sibling->GetNextSibling()) ) {
      if (sibling->Tag() == nsGkAtoms::img) {
        HTMLImageElement *img = static_cast<HTMLImageElement*>(sibling.get());
        img->PictureSourceRemoved(AsContent());
      }
    }
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

JSObject*
HTMLSourceElement::WrapNode(JSContext* aCx)
{
  return HTMLSourceElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
