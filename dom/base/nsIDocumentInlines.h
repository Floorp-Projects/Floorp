/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentInlines_h
#define nsIDocumentInlines_h

#include "nsIDocument.h"
#include "mozilla/dom/HTMLBodyElement.h"

inline mozilla::dom::HTMLBodyElement*
nsIDocument::GetBodyElement()
{
  return static_cast<mozilla::dom::HTMLBodyElement*>(GetHtmlChildElement(nsGkAtoms::body));
}

#endif // nsIDocumentInlines_h
