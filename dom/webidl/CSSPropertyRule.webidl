/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.css-houdini.org/css-properties-values-api-1/#the-css-property-rule-interface
 */

// https://drafts.css-houdini.org/css-properties-values-api-1/#the-css-property-rule-interface
[Exposed=Window, Pref="layout.css.properties-and-values.enabled"]
interface CSSPropertyRule : CSSRule {
  readonly attribute UTF8String name;
  readonly attribute UTF8String syntax;
  readonly attribute boolean inherits;
  readonly attribute UTF8String? initialValue;
};
