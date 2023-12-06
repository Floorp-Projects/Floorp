/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/scripting.html#the-template-element
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=Window]
interface HTMLTemplateElement : HTMLElement {
  [HTMLConstructor] constructor();

  readonly attribute DocumentFragment content;
  [CEReactions, Pref="dom.webcomponents.shadowdom.declarative.enabled"]
  attribute DOMString shadowRootMode;
  [CEReactions, Pref="dom.webcomponents.shadowdom.declarative.enabled"]
  attribute boolean shadowRootDelegatesFocus;
};
