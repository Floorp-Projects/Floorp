/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSummaryElement_h
#define mozilla_dom_HTMLSummaryElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

// HTMLSummaryElement implements the <summary> tag, which is used as a summary
// or legend of the <details> tag. Please see the spec for more information.
// https://html.spec.whatwg.org/multipage/forms.html#the-details-element
//
class HTMLSummaryElement final : public nsGenericHTMLElement
{
public:
  using NodeInfo = mozilla::dom::NodeInfo;

  explicit HTMLSummaryElement(already_AddRefed<NodeInfo>& aNodeInfo)
    : nsGenericHTMLElement(aNodeInfo)
  {
  }

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLSummaryElement, summary)

  nsresult Clone(NodeInfo* aNodeInfo, nsINode** aResult) const override;

protected:
  virtual ~HTMLSummaryElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLSummaryElement_h */
