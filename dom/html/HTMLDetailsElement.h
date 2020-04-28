/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLDetailsElement_h
#define mozilla_dom_HTMLDetailsElement_h

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
namespace dom {

// HTMLDetailsElement implements the <details> tag, which is used as a
// disclosure widget from which the user can obtain additional information or
// controls. Please see the spec for more information.
// https://html.spec.whatwg.org/multipage/forms.html#the-details-element
//
class HTMLDetailsElement final : public nsGenericHTMLElement {
 public:
  using NodeInfo = mozilla::dom::NodeInfo;

  explicit HTMLDetailsElement(already_AddRefed<NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {}

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLDetailsElement, details)

  nsIContent* GetFirstSummary() const;

  nsresult Clone(NodeInfo* aNodeInfo, nsINode** aResult) const override;

  // Element
  nsChangeHint GetAttributeChangeHint(const nsAtom* aAttribute,
                                      int32_t aModType) const override;

  nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                         const nsAttrValueOrString* aValue,
                         bool aNotify) override;

  bool IsInteractiveHTMLContent() const override { return true; }

  // HTMLDetailsElement WebIDL
  bool Open() const { return GetBoolAttr(nsGkAtoms::open); }

  void SetOpen(bool aOpen, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::open, aOpen, aError);
  }

  void ToggleOpen() {
    ErrorResult rv;
    SetOpen(!Open(), rv);
    rv.SuppressException();
  }

  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

 protected:
  virtual ~HTMLDetailsElement();

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

  RefPtr<AsyncEventDispatcher> mToggleEventDispatcher;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_HTMLDetailsElement_h */
