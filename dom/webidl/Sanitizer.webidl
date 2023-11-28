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


dictionary SanitizerElementNamespace {
  required DOMString name;
  DOMString? _namespace = "http://www.w3.org/1999/xhtml";
};

// Used by "elements"
dictionary SanitizerElementNamespaceWithAttributes : SanitizerElementNamespace {
  sequence<SanitizerAttribute> attributes;
  sequence<SanitizerAttribute> removeAttributes;
};

typedef (DOMString or SanitizerElementNamespace) SanitizerElement;
typedef (DOMString or SanitizerElementNamespaceWithAttributes) SanitizerElementWithAttributes;

dictionary SanitizerAttributeNamespace {
  required DOMString name;
  DOMString? _namespace = null;
};
typedef (DOMString or SanitizerAttributeNamespace) SanitizerAttribute;

dictionary SanitizerConfig {
  sequence<SanitizerElementWithAttributes> elements;
  sequence<SanitizerElement> removeElements;
  sequence<SanitizerElement> replaceWithChildrenElements;

  sequence<SanitizerAttribute> attributes;
  sequence<SanitizerAttribute> removeAttributes;

  boolean customElements;
  boolean unknownMarkup; // Name TBD!
  boolean comments;
};

typedef (DocumentFragment or Document) SanitizerInput;

[Exposed=Window, SecureContext, Pref="dom.security.sanitizer.enabled"]
interface Sanitizer {
  [Throws, UseCounter]
  constructor(optional SanitizerConfig sanitizerConfig = {});
  [UseCounter, Throws]
  DocumentFragment sanitize(SanitizerInput input);
};
