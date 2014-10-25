/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLPictureElement.h"
#include "mozilla/dom/HTMLPictureElementBinding.h"
#include "mozilla/dom/HTMLImageElement.h"

#include "mozilla/Preferences.h"
static const char *kPrefPictureEnabled = "dom.image.picture.enabled";

// Expand NS_IMPL_NS_NEW_HTML_ELEMENT(Picture) to add pref check.
nsGenericHTMLElement*
NS_NewHTMLPictureElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                         mozilla::dom::FromParser aFromParser)
{
  if (!mozilla::dom::HTMLPictureElement::IsPictureEnabled()) {
    return new mozilla::dom::HTMLUnknownElement(aNodeInfo);
  }

  return new mozilla::dom::HTMLPictureElement(aNodeInfo);
}

namespace mozilla {
namespace dom {

HTMLPictureElement::HTMLPictureElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLPictureElement::~HTMLPictureElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLPictureElement, nsGenericHTMLElement,
                            nsIDOMHTMLPictureElement)

NS_IMPL_ELEMENT_CLONE(HTMLPictureElement)

void
HTMLPictureElement::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
  // Find all img siblings after this <source> to notify them of its demise
  nsCOMPtr<nsINode> child = GetChildAt(aIndex);
  nsCOMPtr<nsIContent> nextSibling;
  if (child && child->Tag() == nsGkAtoms::source) {
    nextSibling = child->GetNextSibling();
  }

  nsGenericHTMLElement::RemoveChildAt(aIndex, aNotify);

  if (nextSibling && nextSibling->GetParentNode() == this) {
    do {
      HTMLImageElement* img = HTMLImageElement::FromContent(nextSibling);
      if (img) {
        img->PictureSourceRemoved(child->AsContent());
      }
    } while ( (nextSibling = nextSibling->GetNextSibling()) );
  }
}

bool
HTMLPictureElement::IsPictureEnabled()
{
  return HTMLImageElement::IsSrcsetEnabled() &&
         Preferences::GetBool(kPrefPictureEnabled, false);
}

JSObject*
HTMLPictureElement::WrapNode(JSContext* aCx)
{
  return HTMLPictureElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
