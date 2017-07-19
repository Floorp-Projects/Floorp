/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DocumentStyleRootIterator_h
#define DocumentStyleRootIterator_h

#include "nsTArray.h"

class nsIContent;
class nsIDocument;

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

/**
 * DocumentStyleRootIterator traverses the roots of the document from the
 * perspective of the Servo-backed style system. In the general case, this
 * will first traverse the document root, followed by any document level
 * native anonymous content.
 *
 * If the caller passes an element to the constructor rather than the document,
 * that element (and nothing else) is returned from GetNextStyleRoot.
 */
class DocumentStyleRootIterator
{
public:
  explicit DocumentStyleRootIterator(nsINode* aStyleRoot);
  ~DocumentStyleRootIterator() { MOZ_COUNT_DTOR(DocumentStyleRootIterator); }

  dom::Element* GetNextStyleRoot();

private:
  AutoTArray<nsIContent*, 8> mStyleRoots;
  uint32_t mPosition;
};

} // namespace mozilla

#endif // DocumentStyleRootIterator_h
