/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/scroll-animations-1/#scrolltimeline-interface
 */

[Exposed=Window, Pref="layout.css.scroll-linked-animations.enabled"]
interface CSSScrollTimelineRule : CSSRule {
  readonly attribute DOMString name;
  readonly attribute DOMString source;
  readonly attribute DOMString orientation;
  readonly attribute DOMString scrollOffsets;
};
