/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 */

 // Because of getComputedStyle, many CSSStyleDeclaration objects can be
 // short-living.
[ProbablyShortLivingWrapper,
 Exposed=Window]
interface CSSStyleDeclaration {
  [CEReactions, SetterNeedsSubjectPrincipal=NonSystem, SetterThrows]
  attribute UTF8String cssText;

  readonly attribute unsigned long length;
  getter UTF8String item(unsigned long index);

  [Throws, ChromeOnly]
  sequence<UTF8String> getCSSImageURLs(UTF8String property);

  [Throws]
  UTF8String getPropertyValue(UTF8String property);
  UTF8String getPropertyPriority(UTF8String property);
  [CEReactions, NeedsSubjectPrincipal=NonSystem, Throws]
  void setProperty(UTF8String property, [LegacyNullToEmptyString] UTF8String value, optional [LegacyNullToEmptyString] UTF8String priority = "");
  [CEReactions, Throws]
  UTF8String removeProperty(UTF8String property);

  readonly attribute CSSRule? parentRule;
};
