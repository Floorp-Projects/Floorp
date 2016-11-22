/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PaintTracker_h
#define mozilla_PaintTracker_h

#include "mozilla/Attributes.h"
#include "nsDebug.h"

namespace mozilla {

class MOZ_STACK_CLASS PaintTracker
{
public:
  PaintTracker() {
    ++gPaintTracker;
  }
  ~PaintTracker() {
    NS_ASSERTION(gPaintTracker > 0, "Mismatched constructor/destructor");
    --gPaintTracker;
  }

  static bool IsPainting() {
    return !!gPaintTracker;
  }

private:
  static int gPaintTracker;
};

} // namespace mozilla

#endif // mozilla_PaintTracker_h
