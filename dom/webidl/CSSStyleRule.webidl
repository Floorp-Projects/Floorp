/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/cssom/#the-cssstylerule-interface
 */

// https://drafts.csswg.org/cssom/#the-cssstylerule-interface
[Exposed=Window]
interface CSSStyleRule : CSSRule {
  attribute UTF8String selectorText;
  [SameObject, PutForwards=cssText] readonly attribute CSSStyleDeclaration style;

  // https://drafts.csswg.org/css-nesting/#cssom-style
  // FIXME: Ideally CSSStyleRule should inherit from CSSGroupingRule instead,
  // see https://github.com/w3c/csswg-drafts/issues/8940
  [Pref="layout.css.nesting.enabled", SameObject] readonly attribute CSSRuleList cssRules;
  [Pref="layout.css.nesting.enabled", Throws] unsigned long insertRule(UTF8String rule, optional unsigned long index = 0);
  [Pref="layout.css.nesting.enabled", Throws] undefined deleteRule(unsigned long index);

  [ChromeOnly] readonly attribute unsigned long selectorCount;
  [ChromeOnly] UTF8String selectorTextAt(unsigned long index, optional boolean desugared = false);
  [ChromeOnly] unsigned long long selectorSpecificityAt(unsigned long index, optional boolean desugared = false);
  [ChromeOnly] boolean selectorMatchesElement(
    unsigned long selectorIndex,
    Element element,
    optional [LegacyNullToEmptyString] DOMString pseudo = "",
    optional boolean includeVisitedStyle = false);
};
