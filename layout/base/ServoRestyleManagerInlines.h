/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ServoRestyleManagerInlines_h
#define ServoRestyleManagerInlines_h

#include "ServoRestyleManager.h"

#include "mozilla/dom/ElementInlines.h"

namespace mozilla {

using namespace dom;

inline bool
ServoRestyleManager::HasPendingRestyles()
{
  nsIDocument* doc = PresContext()->Document();
  DocumentStyleRootIterator iter(doc);
  while (Element* root = iter.GetNextStyleRoot()) {
    if (root->ShouldTraverseForServo()) {
      return true;
    }
  }
  return false;
}

} // namespace mozilla

#endif // ServoRestyleManagerInlines_h
