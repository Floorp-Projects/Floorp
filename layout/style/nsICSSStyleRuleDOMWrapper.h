/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface to provide DOM inspector with access to internal interfaces
 * via DOM interface
 */

#ifndef nsICSSStyleRuleDOMWrapper_h_
#define nsICSSStyleRuleDOMWrapper_h_

#include "nsIDOMCSSStyleRule.h"

// IID for the nsICSSStyleRuleDOMWrapper interface
// {cee1bbb6-0a32-4cf3-8d42-ba3938e9ecaa}
#define NS_ICSS_STYLE_RULE_DOM_WRAPPER_IID \
{0xcee1bbb6, 0x0a32, 0x4cf3, {0x8d, 0x42, 0xba, 0x39, 0x38, 0xe9, 0xec, 0xaa}}

namespace mozilla {
class BindingStyleRule;
} // namespace mozilla

class nsICSSStyleRuleDOMWrapper : public nsIDOMCSSStyleRule {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_STYLE_RULE_DOM_WRAPPER_IID)

  NS_IMETHOD GetCSSStyleRule(mozilla::BindingStyleRule** aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSStyleRuleDOMWrapper,
                              NS_ICSS_STYLE_RULE_DOM_WRAPPER_IID)

#endif /* !defined(nsICSSStyleRuleDOMWrapper_h_) */
