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
interface CSSStyleRule : CSSGroupingRule {
  attribute UTF8String selectorText;
  [SameObject, PutForwards=cssText] readonly attribute CSSStyleDeclaration style;

  [ChromeOnly] readonly attribute unsigned long selectorCount;
  [ChromeOnly] UTF8String selectorTextAt(unsigned long index, optional boolean desugared = false);
  [ChromeOnly] unsigned long long selectorSpecificityAt(unsigned long index, optional boolean desugared = false);
  [ChromeOnly] boolean selectorMatchesElement(
    unsigned long selectorIndex,
    Element element,
    optional [LegacyNullToEmptyString] DOMString pseudo = "",
    optional boolean includeVisitedStyle = false);
  [ChromeOnly] sequence<SelectorWarning> getSelectorWarnings();
};

enum SelectorWarningKind {
  "UnconstrainedHas",
};

dictionary SelectorWarning {
  required unsigned long index;
  required SelectorWarningKind kind;
};
