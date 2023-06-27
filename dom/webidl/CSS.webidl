/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/css3-conditional/
 * http://dev.w3.org/csswg/cssom/#the-css.escape%28%29-method
 * https://www.w3.org/TR/css-highlight-api-1/#registration
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=Window]
namespace CSS {
  boolean supports(UTF8String property, UTF8String value);
  boolean supports(UTF8String conditionText);
};

// http://dev.w3.org/csswg/cssom/#the-css.escape%28%29-method
partial namespace CSS {
  DOMString escape(DOMString ident);
};

// https://www.w3.org/TR/css-highlight-api-1/#registration
partial namespace CSS {
  [Pref="dom.customHighlightAPI.enabled", GetterThrows]
  readonly attribute HighlightRegistry highlights;
};

// https://drafts.css-houdini.org/css-properties-values-api-1/#registering-custom-properties
// See https://github.com/w3c/css-houdini-drafts/pull/1100 for DOMString vs. UTF8String
dictionary PropertyDefinition {
  required UTF8String name;
           UTF8String syntax       = "*";
  required boolean    inherits;
           UTF8String initialValue;
};
partial namespace CSS {
  [Pref="layout.css.properties-and-values.enabled", Throws]
  undefined registerProperty(PropertyDefinition definition);
};
