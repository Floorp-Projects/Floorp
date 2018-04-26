/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MozQueryInterface
#define mozilla_dom_MozQueryInterface

/**
 * This class implements an optimized QueryInterface method for
 * XPConnect-wrapped JS objects.
 *
 * For JavaScript callers, it behaves as an ordinary QueryInterface method,
 * returning its `this` object or throwing depending on the interface it was
 * passed.
 *
 * For native XPConnect callers, we bypass JSAPI entirely, and directly check
 * whether the queried interface is in the interfaces list.
 */

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

class MozQueryInterface final : public NonRefcountedDOMObject
{
public:
  explicit MozQueryInterface(nsTArray<nsIID>&& aInterfaces)
    : mInterfaces(Move(aInterfaces))
  {}

  bool QueriesTo(const nsIID& aIID) const;

  void LegacyCall(JSContext* cx, JS::Handle<JS::Value> thisv, nsIJSID* aIID, JS::MutableHandle<JS::Value> aResult, ErrorResult& aRv) const;

  nsISupports* GetParentObject() const { return nullptr; }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector);

private:
  nsTArray<nsIID> mInterfaces;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MozQueryInterface
