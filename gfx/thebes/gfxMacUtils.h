/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MAC_UTILS_H
#define GFX_MAC_UTILS_H

#include <CoreFoundation/CoreFoundation.h>
#include "mozilla/gfx/2D.h"

class gfxMacUtils {
 public:
  // This takes a TransferFunction and returns a constant CFStringRef, which
  // uses get semantics and does not need to be released.
  static CFStringRef CFStringForTransferFunction(
      mozilla::gfx::TransferFunction);
};

#endif
