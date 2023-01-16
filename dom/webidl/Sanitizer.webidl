/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://wicg.github.io/sanitizer-api/#sanitizer
 * https://wicg.github.io/sanitizer-api/#config
 *
 * * Copyright © 2020 the Contributors to the HTML Sanitizer API Specification,
 * published by the Web Platform Incubator Community Group under the W3C Community Contributor License Agreement (CLA).
 */

// NOTE: This IDL is still under development:
// https://github.com/WICG/sanitizer-api/issues/181

typedef (DocumentFragment or Document) SanitizerInput;

[GenerateConversionToJS]
dictionary SanitizerElementNamespace {
  required DOMString name;
  required DOMString _namespace;
};

typedef (DOMString or SanitizerElementNamespace) SanitizerElement;

enum Star {
  "*"
};

dictionary SanitizerAttribute {
  required DOMString name;
  DOMString? _namespace = null;
  required (Star or sequence<SanitizerElement>) elements;
};

[Exposed=Window, SecureContext, Pref="dom.security.sanitizer.enabled"]
interface Sanitizer {
  [Throws, UseCounter]
  constructor(optional SanitizerConfig sanitizerConfig = {});
  [UseCounter, Throws]
  DocumentFragment sanitize(SanitizerInput input);
};

dictionary SanitizerConfig {
  sequence<SanitizerElement> allowElements;
  sequence<SanitizerElement> blockElements;
  sequence<SanitizerElement> dropElements;
  sequence<SanitizerAttribute> allowAttributes;
  sequence<SanitizerAttribute> dropAttributes;
  boolean allowCustomElements;
  boolean allowUnknownMarkup;
  boolean allowComments;
};
