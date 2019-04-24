/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary DOMOverlaysError {
  short code;
  DOMString translatedElementName;
  DOMString sourceElementName;
  DOMString l10nName;
};

[ChromeOnly]
namespace DOMOverlays {
  const unsigned short ERROR_FORBIDDEN_TYPE = 1;
  const unsigned short ERROR_NAMED_ELEMENT_MISSING = 2;
  const unsigned short ERROR_NAMED_ELEMENT_TYPE_MISMATCH = 3;
  const unsigned short ERROR_UNKNOWN = 4;

  sequence<DOMOverlaysError>? translateElement(Element element, optional L10nValue translation);
};
