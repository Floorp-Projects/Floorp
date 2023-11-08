/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://github.com/whatwg/html/pull/9841
 */

interface mixin InvokerElement {
  [Pref="dom.element.invokers.enabled", CEReactions] attribute Element? invokeTargetElement;
  [Pref="dom.element.invokers.enabled", CEReactions] attribute DOMString invokeAction;
};
