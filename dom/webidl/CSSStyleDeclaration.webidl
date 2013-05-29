/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 */

interface CSSRule;

interface CSSStyleDeclaration {
  [SetterThrows]
  attribute DOMString cssText;

  readonly attribute unsigned long length;
  getter DOMString item(unsigned long index);

  [Throws]
  DOMString getPropertyValue(DOMString property);
  // Mozilla extension, sort of
  [Throws]
  CSSValue? getPropertyCSSValue(DOMString property);
  DOMString getPropertyPriority(DOMString property);
  [Throws]
  void setProperty(DOMString property, DOMString value, [TreatNullAs=EmptyString] optional DOMString priority = "");
  [Throws]
  DOMString removeProperty(DOMString property);

  readonly attribute CSSRule? parentRule;
};
