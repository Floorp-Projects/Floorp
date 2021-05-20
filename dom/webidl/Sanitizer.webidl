/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://wicg.github.io/purification/
 *
 * * Copyright © 2020 the Contributors to the HTML Sanitizer API Specification,
 * published by the Web Platform Incubator Community Group under the W3C Community Contributor License Agreement (CLA).
 */


typedef (DOMString or DocumentFragment or Document) SanitizerInput;
typedef record<DOMString, sequence<DOMString>> AttributeMatchList;

[Exposed=Window, SecureContext, Pref="dom.security.sanitizer.enabled"]
interface Sanitizer {
  [Throws]
  constructor(optional SanitizerConfig sanitizerConfig = {});
  [Throws]
  DocumentFragment sanitize(SanitizerInput input);
  [Throws]
  DOMString sanitizeToString(SanitizerInput input);
};

dictionary SanitizerConfig {
  sequence<DOMString> allowElements;
  sequence<DOMString> blockElements;
  sequence<DOMString> dropElements;
  AttributeMatchList allowAttributes;
  AttributeMatchList dropAttributes;
  boolean allowCustomElements;
};
