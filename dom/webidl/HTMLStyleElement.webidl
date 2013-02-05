/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-style-element
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
 */

interface HTMLStyleElement : HTMLElement {
           [SetterThrows]
           attribute boolean disabled;
           [SetterThrows]
           attribute DOMString media;
           [SetterThrows]
           attribute DOMString type;
           [SetterThrows]
           attribute boolean scoped;
};
HTMLStyleElement implements LinkStyle;

