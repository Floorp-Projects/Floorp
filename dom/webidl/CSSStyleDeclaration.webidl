/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 */

interface CSSRule;
interface CSSValue;

interface CSSStyleDeclaration {
  [GetterInfallible]
  attribute DOMString cssText;

  [Infallible]
  readonly attribute unsigned long length;
  [Infallible]
  getter DOMString item(unsigned long index);

  DOMString getPropertyValue(DOMString property);
  // Mozilla extension, sort of
  CSSValue getPropertyCSSValue(DOMString property);
  [Infallible]
  DOMString getPropertyPriority(DOMString property);
  // This would be nicer if it used a string default value of "".
  // See bug 759622.
  void setProperty(DOMString property, DOMString value, [TreatNullAs=EmptyString] optional DOMString priority);
  DOMString removeProperty(DOMString property);

  [Infallible]
  readonly attribute CSSRule parentRule;
};
