/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/grouping-content.html#the-menu-element
 * https://html.spec.whatwg.org/multipage/obsolete.html#HTMLMenuElement-partial
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

// https://html.spec.whatwg.org/multipage/grouping-content.html#the-menu-element
[Exposed=Window]
interface HTMLMenuElement : HTMLElement {
  [HTMLConstructor] constructor();
};

// https://html.spec.whatwg.org/multipage/obsolete.html#HTMLMenuElement-partial
partial interface HTMLMenuElement {
           [CEReactions, SetterThrows]
           attribute boolean compact;
};
