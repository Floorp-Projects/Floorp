/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDialogElement.h"
#include "mozilla/dom/HTMLDialogElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Dialog)

namespace mozilla {
namespace dom {

HTMLDialogElement::~HTMLDialogElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLDialogElement)

void
HTMLDialogElement::Close(const mozilla::dom::Optional<nsAString>& aReturnValue)
{
  if (!Open()) {
    return;
  }
  if (aReturnValue.WasPassed()) {
    SetReturnValue(aReturnValue.Value());
  }
  ErrorResult ignored;
  SetOpen(false, ignored);
  ignored.SuppressException();
  RefPtr<AsyncEventDispatcher> eventDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("close"), false);
  eventDispatcher->PostDOMEvent();
}

void
HTMLDialogElement::Show()
{
  if (Open()) {
    return;
  }
  ErrorResult ignored;
  SetOpen(true, ignored);
  ignored.SuppressException();
}

void
HTMLDialogElement::ShowModal(ErrorResult& aError)
{
  if (!IsInComposedDoc() || Open()) {
   aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
   return;
  }

  SetOpen(true, aError);
  aError.SuppressException();
}

JSObject*
HTMLDialogElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLDialogElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
