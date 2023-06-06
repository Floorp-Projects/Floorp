/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-textarea-element
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

interface nsIEditor;
interface XULControllers;

[Exposed=Window]
interface HTMLTextAreaElement : HTMLElement {
  [HTMLConstructor] constructor();

  [CEReactions, SetterThrows, Pure]
           attribute DOMString autocomplete;
  [CEReactions, SetterThrows, Pure]
           attribute unsigned long cols;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString dirName;
  [CEReactions, SetterThrows, Pure]
           attribute boolean disabled;
  [Pure]
  readonly attribute HTMLFormElement? form;
           // attribute DOMString inputMode;
  [CEReactions, SetterThrows, Pure]
           attribute long maxLength;
  [CEReactions, SetterThrows, Pure]
           attribute long minLength;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString name;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString placeholder;
  [CEReactions, SetterThrows, Pure]
           attribute boolean readOnly;
  [CEReactions, SetterThrows, Pure]
           attribute boolean required;
  [CEReactions, SetterThrows, Pure]
           attribute unsigned long rows;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString wrap;

  [Constant]
  readonly attribute DOMString type;
  [CEReactions, Throws, Pure]
           attribute DOMString defaultValue;
  [CEReactions, SetterThrows] attribute [LegacyNullToEmptyString] DOMString value;
  [BinaryName="getTextLength"]
  readonly attribute unsigned long textLength;

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  [Throws]
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  undefined setCustomValidity(DOMString error);

  readonly attribute NodeList labels;

  undefined select();
  [Throws]
           attribute unsigned long? selectionStart;
  [Throws]
           attribute unsigned long? selectionEnd;
  [Throws]
           attribute DOMString? selectionDirection;
  [Throws]
  undefined setRangeText(DOMString replacement);
  [Throws]
  undefined setRangeText(DOMString replacement, unsigned long start,
    unsigned long end, optional SelectionMode selectionMode = "preserve");
  [Throws]
  undefined setSelectionRange(unsigned long start, unsigned long end, optional DOMString direction);
};

partial interface HTMLTextAreaElement {
  // Chrome-only Mozilla extensions

  [Throws, ChromeOnly]
  readonly attribute XULControllers controllers;
};

HTMLTextAreaElement includes MozEditableElement;

partial interface HTMLTextAreaElement {
  [ChromeOnly]
  attribute DOMString previewValue;
};
