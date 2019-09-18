/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextEditUtils_h
#define TextEditUtils_h

#include "nscore.h"

class nsINode;

namespace mozilla {

class TextEditor;

class TextEditUtils final {
 public:
  static bool IsBreak(nsINode* aNode);
};

}  // namespace mozilla

#endif  // #ifndef TextEditUtils_h
