/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BindingStyleRule.h"
#include "mozilla/dom/CSSStyleRuleBinding.h"

namespace mozilla {

/* virtual */ JSObject*
BindingStyleRule::WrapObject(JSContext* aCx,
			     JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSStyleRuleBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace mozilla
