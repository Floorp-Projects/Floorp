/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
