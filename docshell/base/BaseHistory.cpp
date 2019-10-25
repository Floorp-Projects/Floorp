/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseHistory.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/Element.h"

namespace mozilla {

dom::Document* BaseHistory::GetLinkDocument(dom::Link& aLink) {
  Element* element = aLink.GetElement();
  // Element can only be null for mock_Link.
  return element ? element->OwnerDoc() : nullptr;
}

} // namespace mozilla
