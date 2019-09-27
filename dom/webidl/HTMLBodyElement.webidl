/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

[HTMLConstructor,
 Exposed=Window]
interface HTMLBodyElement : HTMLElement {
};

partial interface HTMLBodyElement {
  [CEReactions, SetterThrows]
  attribute [TreatNullAs=EmptyString] DOMString text;
  [CEReactions, SetterThrows]
  attribute [TreatNullAs=EmptyString] DOMString link;
  [CEReactions, SetterThrows]
  attribute [TreatNullAs=EmptyString] DOMString vLink;
  [CEReactions, SetterThrows]
  attribute [TreatNullAs=EmptyString] DOMString aLink;
  [CEReactions, SetterThrows]
  attribute [TreatNullAs=EmptyString] DOMString bgColor;
  [CEReactions, SetterThrows]
  attribute DOMString background;
};

HTMLBodyElement includes WindowEventHandlers;
