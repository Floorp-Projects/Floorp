/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/RadioNodeList.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/RadioNodeListBinding.h"
#include "js/TypeDecls.h"

#include "HTMLInputElement.h"

namespace mozilla {
namespace dom {

/* virtual */ JSObject*
RadioNodeList::WrapObject(JSContext* aCx)
{
  return RadioNodeListBinding::Wrap(aCx, this);
}

HTMLInputElement*
GetAsRadio(nsIContent* node)
{
  HTMLInputElement* el = HTMLInputElement::FromContent(node);
  if (el && el->GetType() == NS_FORM_INPUT_RADIO) {
    return el;
  }
  return nullptr;
}

void
RadioNodeList::GetValue(nsString& retval)
{
  for (uint32_t i = 0; i < Length(); i++) {
    HTMLInputElement* maybeRadio = GetAsRadio(Item(i));
    if (maybeRadio && maybeRadio->Checked()) {
      maybeRadio->GetValue(retval);
      return;
    }
  }
  retval.Truncate();
}

void
RadioNodeList::SetValue(const nsAString& value)
{
  for (uint32_t i = 0; i < Length(); i++) {

    HTMLInputElement* maybeRadio = GetAsRadio(Item(i));
    if (!maybeRadio) {
      continue;
    }

    nsString curval = nsString();
    maybeRadio->GetValue(curval);
    if (curval.Equals(value)) {
      maybeRadio->SetChecked(true);
      return;
    }

  }
}

NS_IMPL_ISUPPORTS_INHERITED(RadioNodeList, nsSimpleContentList, RadioNodeList)

} // namespace dom
} // namespace mozilla
