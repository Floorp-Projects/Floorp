/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSImportRule.h"

#include "mozilla/dom/CSSImportRuleBinding.h"
#include "mozilla/dom/MediaList.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(CSSImportRule, css::Rule)
NS_IMPL_RELEASE_INHERITED(CSSImportRule, css::Rule)

// QueryInterface implementation for CSSImportRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CSSImportRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSImportRule)
NS_INTERFACE_MAP_END_INHERITING(css::Rule)

bool
CSSImportRule::IsCCLeaf() const
{
  // We're not a leaf.
  return false;
}

NS_IMETHODIMP
CSSImportRule::GetMedia(nsIDOMMediaList** aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  NS_ADDREF(*aMedia = Media());
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRule::GetStyleSheet(nsIDOMCSSStyleSheet** aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);
  NS_IF_ADDREF(*aStyleSheet = GetStyleSheet());
  return NS_OK;
}

/* virtual */ JSObject*
CSSImportRule::WrapObject(JSContext* aCx,
                          JS::Handle<JSObject*> aGivenProto)
{
  return CSSImportRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
