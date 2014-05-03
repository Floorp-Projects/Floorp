/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICSSRuleList.h"

#include "mozilla/dom/CSSRuleListBinding.h"

using namespace mozilla;


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsICSSRuleList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsICSSRuleList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsICSSRuleList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsICSSRuleList)

/* virtual */ JSObject*
nsICSSRuleList::WrapObject(JSContext* aCx)
{
  return dom::CSSRuleListBinding::Wrap(aCx, this);
}
