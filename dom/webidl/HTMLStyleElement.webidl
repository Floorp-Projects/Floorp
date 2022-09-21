/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-style-element
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
 */

[Exposed=Window]
interface HTMLStyleElement : HTMLElement {
  [HTMLConstructor] constructor();

           [Pure]
           attribute boolean disabled;
           [CEReactions, SetterThrows, Pure]
           attribute DOMString media;
           [CEReactions, SetterThrows, Pure]
           attribute DOMString type;
};
HTMLStyleElement includes LinkStyle;

// Mozilla-specific additions to support devtools
partial interface HTMLStyleElement {
  /**
   * Mark this style element with a devtools-specific principal that
   * skips Content Security Policy unsafe-inline checks. This triggering
   * principal will be overwritten by any callers that set textContent
   * or innerHTML on this element.
   */
  [ChromeOnly]
  undefined setDevtoolsAsTriggeringPrincipal();
};
