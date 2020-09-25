/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Types.h"

#include "nsPrintfCString.h"

#include <ostream>

namespace mozilla {
namespace gfx {

std::ostream& operator<<(std::ostream& aOut, const DeviceColor& aColor) {
  aOut << nsPrintfCString("dev_rgba(%d, %d, %d, %f)", uint8_t(aColor.r * 255.f),
                          uint8_t(aColor.g * 255.f), uint8_t(aColor.b * 255.f),
                          aColor.a)
              .get();
  return aOut;
}

}  // namespace gfx
}  // namespace mozilla
