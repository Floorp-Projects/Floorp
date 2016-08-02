/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 */

interface CSSRule;

enum CSSStyleSheetParsingMode {
  "author",
  "user",
  "agent"
};

interface CSSStyleSheet : StyleSheet {
  [Pure]
  readonly attribute CSSRule? ownerRule;
  [Throws]
  readonly attribute CSSRuleList cssRules;
  [ChromeOnly, BinaryName="parsingModeDOM"]
  readonly attribute CSSStyleSheetParsingMode parsingMode;
  [Throws]
  unsigned long insertRule(DOMString rule, unsigned long index);
  [Throws]
  void deleteRule(unsigned long index);
};
