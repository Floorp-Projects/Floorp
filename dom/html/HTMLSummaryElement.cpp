/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSummaryElement.h"

#include "mozilla/dom/HTMLDetailsElement.h"
#include "mozilla/dom/HTMLElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Summary)

namespace mozilla {
namespace dom {

HTMLSummaryElement::~HTMLSummaryElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLSummaryElement)

bool
HTMLSummaryElement::IsMainSummary() const
{
  HTMLDetailsElement* details = GetDetails();
  if (!details) {
    return false;
  }

  return details->GetFirstSummary() == this || IsRootOfNativeAnonymousSubtree();
}

HTMLDetailsElement*
HTMLSummaryElement::GetDetails() const
{
  return HTMLDetailsElement::FromContentOrNull(GetParent());
}

JSObject*
HTMLSummaryElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
