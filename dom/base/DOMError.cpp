/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMErrorBinding.h"
#include "mozilla/dom/DOMException.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMError, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMError)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(DOMError)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMError::DOMError(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
}

DOMError::DOMError(nsPIDOMWindow* aWindow, nsresult aValue)
  : mWindow(aWindow)
{
  nsCString name, message;
  NS_GetNameAndMessageForDOMNSResult(aValue, name, message);

  CopyUTF8toUTF16(name, mName);
  CopyUTF8toUTF16(message, mMessage);
}

DOMError::DOMError(nsPIDOMWindow* aWindow, const nsAString& aName)
  : mWindow(aWindow)
  , mName(aName)
{
}

DOMError::DOMError(nsPIDOMWindow* aWindow, const nsAString& aName,
                   const nsAString& aMessage)
  : mWindow(aWindow)
  , mName(aName)
  , mMessage(aMessage)
{
}

DOMError::~DOMError()
{
}

JSObject*
DOMError::WrapObject(JSContext* aCx)
{
  return DOMErrorBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<DOMError>
DOMError::Constructor(const GlobalObject& aGlobal,
                      const nsAString& aName, const nsAString& aMessage,
                      ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());

  // Window is null for chrome code.

  nsRefPtr<DOMError> ret = new DOMError(window, aName, aMessage);
  return ret.forget();
}

} // namespace dom
} // namespace mozilla
