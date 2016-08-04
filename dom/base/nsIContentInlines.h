/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIContentInlines_h
#define nsIContentInlines_h

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"

inline bool
nsIContent::IsInHTMLDocument() const
{
  return OwnerDoc()->IsHTMLDocument();
}

inline bool
nsIContent::IsInChromeDocument()
{
  return nsContentUtils::IsChromeDoc(OwnerDoc());
}

#endif // nsIContentInlines_h
