/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-cascade-5/#the-csslayerstatementrule-interface
 */
[Exposed=Window]
interface CSSLayerStatementRule : CSSRule {
  // readonly attribute FrozenArray<CSSOMString> nameList;
  [Frozen, Cached, Pure]
  readonly attribute sequence<UTF8String> nameList;
};
