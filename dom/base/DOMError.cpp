/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMError.h"

#include "nsDOMClassInfo.h"
#include "nsDOMException.h"

using mozilla::dom::DOMError;

namespace {

struct NameMap
{
  uint16_t code;
  const char* name;
};

} // anonymous namespace

// static
already_AddRefed<nsIDOMDOMError>
DOMError::CreateForNSResult(nsresult aRv)
{
  const char* name;
  const char* message;
  aRv = NS_GetNameAndMessageForDOMNSResult(aRv, &name, &message);
  if (NS_FAILED(aRv) || !name) {
    return nullptr;
  }
  return CreateWithName(NS_ConvertASCIItoUTF16(name));
}

NS_IMPL_ADDREF(DOMError)
NS_IMPL_RELEASE(DOMError)

NS_INTERFACE_MAP_BEGIN(DOMError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMError)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMCI_DATA(DOMError, DOMError)

NS_IMETHODIMP
DOMError::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}
