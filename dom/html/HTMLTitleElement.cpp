/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTitleElement.h"

#include "mozilla/dom/HTMLTitleElementBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsStyleConsts.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Title)

namespace mozilla::dom {

HTMLTitleElement::HTMLTitleElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {
  AddMutationObserver(this);
}

HTMLTitleElement::~HTMLTitleElement() = default;

NS_IMPL_ISUPPORTS_INHERITED(HTMLTitleElement, nsGenericHTMLElement,
                            nsIMutationObserver)

NS_IMPL_ELEMENT_CLONE(HTMLTitleElement)

JSObject* HTMLTitleElement::WrapNode(JSContext* cx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return HTMLTitleElement_Binding::Wrap(cx, this, aGivenProto);
}

void HTMLTitleElement::GetText(DOMString& aText, ErrorResult& aError) const {
  if (!nsContentUtils::GetNodeTextContent(this, false, aText, fallible)) {
    aError = NS_ERROR_OUT_OF_MEMORY;
  }
}

void HTMLTitleElement::SetText(const nsAString& aText, ErrorResult& aError) {
  aError = nsContentUtils::SetNodeTextContent(this, aText, true);
}

void HTMLTitleElement::CharacterDataChanged(nsIContent* aContent,
                                            const CharacterDataChangeInfo&) {
  SendTitleChangeEvent(false);
}

void HTMLTitleElement::ContentAppended(nsIContent* aFirstNewContent) {
  SendTitleChangeEvent(false);
}

void HTMLTitleElement::ContentInserted(nsIContent* aChild) {
  SendTitleChangeEvent(false);
}

void HTMLTitleElement::ContentRemoved(nsIContent* aChild,
                                      nsIContent* aPreviousSibling) {
  SendTitleChangeEvent(false);
}

nsresult HTMLTitleElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  // Let this fall through.
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  SendTitleChangeEvent(true);

  return NS_OK;
}

void HTMLTitleElement::UnbindFromTree(bool aNullParent) {
  SendTitleChangeEvent(false);

  // Let this fall through.
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLTitleElement::DoneAddingChildren(bool aHaveNotified) {
  if (!aHaveNotified) {
    SendTitleChangeEvent(false);
  }
}

void HTMLTitleElement::SendTitleChangeEvent(bool aBound) {
  Document* doc = GetUncomposedDoc();
  if (doc) {
    doc->NotifyPossibleTitleChange(aBound);
  }
}

}  // namespace mozilla::dom
