/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/#elementinternals
 */

[Pref="dom.webcomponents.elementInternals.enabled", Exposed=Window]
interface ElementInternals {
  // Shadow root access
  readonly attribute ShadowRoot? shadowRoot;

  // Form-associated custom elements
  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  void setFormValue((File or USVString or FormData)? value,
                    optional (File or USVString or FormData)? state);

  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  readonly attribute HTMLFormElement? form;

  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  void setValidity(optional ValidityStateFlags flags = {},
                   optional DOMString message,
                   optional HTMLElement anchor);
  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  readonly attribute boolean willValidate;
  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  readonly attribute ValidityState validity;
  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  readonly attribute DOMString validationMessage;

  [Pref="dom.webcomponents.formAssociatedCustomElement.enabled", Throws]
  readonly attribute NodeList labels;
};

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
