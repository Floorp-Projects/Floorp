/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimeStamp.h"
#include "mozilla/dom/Nullable.h"

namespace mozilla {
namespace dom {

class AnimationUtils
{
public:
  static Nullable<double>
    TimeDurationToDouble(const Nullable<TimeDuration>& aTime)
  {
    Nullable<double> result;

    if (!aTime.IsNull()) {
      result.SetValue(aTime.Value().ToMilliseconds());
    }

    return result;
  }
};

} // namespace dom
} // namespace mozilla
