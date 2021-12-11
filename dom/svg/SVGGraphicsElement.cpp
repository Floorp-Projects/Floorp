/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGraphicsElement.h"

#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"

#include "nsIContentInlines.h"

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGGraphicsElement, SVGGraphicsElementBase)
NS_IMPL_RELEASE_INHERITED(SVGGraphicsElement, SVGGraphicsElementBase)

NS_INTERFACE_MAP_BEGIN(SVGGraphicsElement)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::SVGTests)
NS_INTERFACE_MAP_END_INHERITING(SVGGraphicsElementBase)

//----------------------------------------------------------------------
// Implementation

SVGGraphicsElement::SVGGraphicsElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGraphicsElementBase(std::move(aNodeInfo)) {}

bool SVGGraphicsElement::IsSVGFocusable(bool* aIsFocusable,
                                        int32_t* aTabIndex) {
  // XXXedgar, maybe we could factor out the common code for SVG, HTML and
  // MathML elements, see bug 1586011.
  if (!IsInComposedDoc() || IsInDesignMode()) {
    // In designMode documents we only allow focusing the document.
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;

    return true;
  }

  int32_t tabIndex = TabIndex();

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  // If a tabindex is specified at all, or the default tabindex is 0, we're
  // focusable
  *aIsFocusable = tabIndex >= 0 || GetTabIndexAttrValue().isSome();

  return false;
}

bool SVGGraphicsElement::IsFocusableInternal(int32_t* aTabIndex,
                                             bool aWithMouse) {
  bool isFocusable = false;
  IsSVGFocusable(&isFocusable, aTabIndex);
  return isFocusable;
}

nsresult SVGGraphicsElement::BindToTree(BindContext& aContext,
                                        nsINode& aParent) {
  nsresult rv = SVGGraphicsElementBase::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsFocusable() && HasAttr(nsGkAtoms::autofocus) &&
      aContext.AllowsAutoFocus()) {
    aContext.OwnerDoc().SetAutoFocusElement(this);
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
