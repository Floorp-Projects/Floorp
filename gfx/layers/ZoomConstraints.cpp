/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ZoomConstraints.h"

#include <ostream>
#include "nsPrintfCString.h"

namespace mozilla {
namespace layers {

std::ostream& operator<<(std::ostream& aStream, const ZoomConstraints& z) {
  return aStream << nsPrintfCString("{ z=%d, dt=%d, min=%f, max=%f }",
                                    z.mAllowZoom, z.mAllowDoubleTapZoom,
                                    z.mMinZoom.scale, z.mMaxZoom.scale)
                        .get();
}

}  // namespace layers
}  // namespace mozilla
