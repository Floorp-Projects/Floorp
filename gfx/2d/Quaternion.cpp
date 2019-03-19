/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Quaternion.h"
#include "Matrix.h"
#include "Tools.h"
#include <algorithm>
#include <ostream>
#include <math.h>

using namespace std;

namespace mozilla {
namespace gfx {

std::ostream& operator<<(std::ostream& aStream, const Quaternion& aQuat) {
  return aStream << "< " << aQuat.x << " " << aQuat.y << " " << aQuat.z << " "
                 << aQuat.w << ">";
}

}  // namespace gfx
}  // namespace mozilla
