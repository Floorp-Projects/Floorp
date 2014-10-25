/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOMSettableTokenList specified by HTML5.
 */

#include "nsDOMSettableTokenList.h"
#include "mozilla/dom/DOMSettableTokenListBinding.h"
#include "mozilla/dom/Element.h"

void
nsDOMSettableTokenList::SetValue(const nsAString& aValue, mozilla::ErrorResult& rv)
{
  if (!mElement) {
    return;
  }

  rv = mElement->SetAttr(kNameSpaceID_None, mAttrAtom, aValue, true);
}

JSObject*
nsDOMSettableTokenList::WrapObject(JSContext *cx)
{
  return mozilla::dom::DOMSettableTokenListBinding::Wrap(cx, this);
}
