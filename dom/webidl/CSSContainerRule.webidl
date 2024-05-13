/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-contain-3/#the-csscontainerrule-interface
 */

// https://drafts.csswg.org/css-contain-3/#the-csscontainerrule-interface
[Exposed=Window, Pref="layout.css.container-queries.enabled"]
interface CSSContainerRule : CSSConditionRule {
  readonly attribute UTF8String containerName;
  readonly attribute UTF8String containerQuery;

  // Performs a container query look-up for an element.
  [ChromeOnly] Element? queryContainerFor(Element element);
};
