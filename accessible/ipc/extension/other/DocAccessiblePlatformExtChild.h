/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessiblePlatformExtChild_h
#define mozilla_a11y_DocAccessiblePlatformExtChild_h

#include "mozilla/a11y/PDocAccessiblePlatformExtChild.h"

namespace mozilla {
namespace a11y {

class DocAccessibleChild;

class DocAccessiblePlatformExtChild : public PDocAccessiblePlatformExtChild {};
}  // namespace a11y
}  // namespace mozilla

#endif