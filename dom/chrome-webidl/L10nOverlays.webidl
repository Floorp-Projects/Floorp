/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary L10nOverlaysError {
  short code;
  DOMString translatedElementName;
  DOMString sourceElementName;
  DOMString l10nName;
};

[ChromeOnly,
 Exposed=Window]
namespace L10nOverlays {
  const unsigned short ERROR_UNKNOWN = 1;
  const unsigned short ERROR_FORBIDDEN_TYPE = 2;
  const unsigned short ERROR_NAMED_ELEMENT_MISSING = 3;
  const unsigned short ERROR_NAMED_ELEMENT_TYPE_MISMATCH = 4;
  const unsigned short ERROR_TRANSLATED_ELEMENT_DISCONNECTED = 5;
  const unsigned short ERROR_TRANSLATED_ELEMENT_DISALLOWED_DOM = 6;

  sequence<L10nOverlaysError>? translateElement(Element element, optional L10nMessage translation = {});
};
