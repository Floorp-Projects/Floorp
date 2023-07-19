/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 * https://wicg.github.io/construct-stylesheets/
 */

enum CSSStyleSheetParsingMode {
  "author",
  "user",
  "agent"
};

dictionary CSSStyleSheetInit {
  (MediaList or UTF8String) media = "";
  boolean disabled = false;
  UTF8String baseURL;
};

[Exposed=Window]
interface CSSStyleSheet : StyleSheet {
  [Throws]
  constructor(optional CSSStyleSheetInit options = {});
  [Pure, BinaryName="DOMOwnerRule"]
  readonly attribute CSSRule? ownerRule;
  [Throws, NeedsSubjectPrincipal]
  readonly attribute CSSRuleList cssRules;
  [ChromeOnly, BinaryName="parsingModeDOM"]
  readonly attribute CSSStyleSheetParsingMode parsingMode;
  [Throws, NeedsSubjectPrincipal]
  unsigned long insertRule(UTF8String rule, optional unsigned long index = 0);
  [Throws, NeedsSubjectPrincipal]
  undefined deleteRule(unsigned long index);
  [NewObject]
  Promise<CSSStyleSheet> replace(UTF8String text);
  [Throws]
  undefined replaceSync(UTF8String text);

  // Non-standard WebKit things.
  [Throws, NeedsSubjectPrincipal, BinaryName="cssRules"]
  readonly attribute CSSRuleList rules;
  [Throws, NeedsSubjectPrincipal, BinaryName="deleteRule"]
  undefined removeRule(optional unsigned long index = 0);
  [Throws, NeedsSubjectPrincipal]
  long addRule(optional UTF8String selector = "undefined", optional UTF8String style = "undefined", optional unsigned long index);
};
