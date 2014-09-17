/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMMIError.h"
#include "mozilla/dom/DOMMMIErrorBinding.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMMIError)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMMMIError, DOMError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMMIError, DOMError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMMMIError)
NS_INTERFACE_MAP_END_INHERITING(DOMError)

NS_IMPL_ADDREF_INHERITED(DOMMMIError, DOMError)
NS_IMPL_RELEASE_INHERITED(DOMMMIError, DOMError)

DOMMMIError::DOMMMIError(nsPIDOMWindow* aWindow, const nsAString& aName,
                         const nsAString& aMessage, const nsAString& aServiceCode,
                         const Nullable<int16_t>& aInfo)
  : DOMError(aWindow, aName, aMessage)
  , mServiceCode(aServiceCode)
  , mInfo(aInfo)
{
}

JSObject*
DOMMMIError::WrapObject(JSContext* aCx)
{
  return DOMMMIErrorBinding::Wrap(aCx, this);
}
