/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTitleElement.h"

#include "mozilla/dom/HTMLTitleElementBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsStyleConsts.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"


NS_IMPL_NS_NEW_HTML_ELEMENT(Title)

namespace mozilla {
namespace dom {

HTMLTitleElement::HTMLTitleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  AddMutationObserver(this);
}

HTMLTitleElement::~HTMLTitleElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLTitleElement, nsGenericHTMLElement,
                            nsIMutationObserver)

NS_IMPL_ELEMENT_CLONE(HTMLTitleElement)

JSObject*
HTMLTitleElement::WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLTitleElementBinding::Wrap(cx, this, aGivenProto);
}

void
HTMLTitleElement::GetText(DOMString& aText, ErrorResult& aError)
{
  if (!nsContentUtils::GetNodeTextContent(this, false, aText)) {
    aError = NS_ERROR_OUT_OF_MEMORY;
  }
}

void
HTMLTitleElement::SetText(const nsAString& aText, ErrorResult& aError)
{
  aError = nsContentUtils::SetNodeTextContent(this, aText, true);
}

void
HTMLTitleElement::CharacterDataChanged(nsIDocument *aDocument,
                                       nsIContent *aContent,
                                       CharacterDataChangeInfo *aInfo)
{
  SendTitleChangeEvent(false);
}

void
HTMLTitleElement::ContentAppended(nsIDocument *aDocument,
                                  nsIContent *aContainer,
                                  nsIContent *aFirstNewContent,
                                  int32_t aNewIndexInContainer)
{
  SendTitleChangeEvent(false);
}

void
HTMLTitleElement::ContentInserted(nsIDocument *aDocument,
                                  nsIContent *aContainer,
                                  nsIContent *aChild,
                                  int32_t aIndexInContainer)
{
  SendTitleChangeEvent(false);
}

void
HTMLTitleElement::ContentRemoved(nsIDocument *aDocument,
                                 nsIContent *aContainer,
                                 nsIContent *aChild,
                                 int32_t aIndexInContainer,
                                 nsIContent *aPreviousSibling)
{
  SendTitleChangeEvent(false);
}

nsresult
HTMLTitleElement::BindToTree(nsIDocument *aDocument,
                             nsIContent *aParent,
                             nsIContent *aBindingParent,
                             bool aCompileEventHandlers)
{
  // Let this fall through.
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  SendTitleChangeEvent(true);

  return NS_OK;
}

void
HTMLTitleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  SendTitleChangeEvent(false);

  // Let this fall through.
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLTitleElement::DoneAddingChildren(bool aHaveNotified)
{
  if (!aHaveNotified) {
    SendTitleChangeEvent(false);
  }
}

void
HTMLTitleElement::SendTitleChangeEvent(bool aBound)
{
  nsIDocument* doc = GetUncomposedDoc();
  if (doc) {
    doc->NotifyPossibleTitleChange(aBound);
  }
}

} // namespace dom
} // namespace mozilla
