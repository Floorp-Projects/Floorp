/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSRuleList.h"

#include "mozilla/dom/CSSRuleListBinding.h"

namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(CSSRuleList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSRuleList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CSSRuleList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CSSRuleList)

/* virtual */ JSObject*
CSSRuleList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSRuleListBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
