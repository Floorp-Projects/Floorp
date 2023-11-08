/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/#elementinternals
 */

[Exposed=Window]
interface ElementInternals {
  // Shadow root access
  readonly attribute ShadowRoot? shadowRoot;

  // Form-associated custom elements
  [Throws]
  undefined setFormValue((File or USVString or FormData)? value,
                         optional (File or USVString or FormData)? state);

  [Throws]
  readonly attribute HTMLFormElement? form;

  [Throws]
  undefined setValidity(optional ValidityStateFlags flags = {},
                        optional DOMString message,
                        optional HTMLElement anchor);
  [Throws]
  readonly attribute boolean willValidate;
  [Throws]
  readonly attribute ValidityState validity;
  [Throws]
  readonly attribute DOMString validationMessage;
  [Throws]
  boolean checkValidity();
  [Throws]
  boolean reportValidity();

  [Throws]
  readonly attribute NodeList labels;

  [Pref="dom.element.customstateset.enabled", SameObject] readonly attribute CustomStateSet states;
};

[Pref="dom.element.customstateset.enabled", Exposed=Window]
interface CustomStateSet {
  setlike<DOMString>;
};

partial interface CustomStateSet {
  // Setlike methods need to be overriden.
  [Throws]
  undefined add(DOMString state);

  [Throws]
  boolean delete(DOMString state);

  [Throws]
  undefined clear();
};

partial interface ElementInternals {
  [ChromeOnly, Throws]
  readonly attribute HTMLElement? validationAnchor;
};

ElementInternals includes AccessibilityRole;
ElementInternals includes AriaAttributes;

dictionary ValidityStateFlags {
  boolean valueMissing = false;
  boolean typeMismatch = false;
  boolean patternMismatch = false;
  boolean tooLong = false;
  boolean tooShort = false;
  boolean rangeUnderflow = false;
  boolean rangeOverflow = false;
  boolean stepMismatch = false;
  boolean badInput = false;
  boolean customError = false;
};
