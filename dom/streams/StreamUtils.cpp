/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamUtils.h"

#include <cmath>
#include "mozilla/dom/QueuingStrategyBinding.h"

namespace mozilla::dom {

// Streams Spec: 7.4
// https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
double ExtractHighWaterMark(const QueuingStrategy& aStrategy,
                            double aDefaultHWM, mozilla::ErrorResult& aRv) {
  // Step 1.
  if (!aStrategy.mHighWaterMark.WasPassed()) {
    return aDefaultHWM;
  }

  // Step 2.
  double highWaterMark = aStrategy.mHighWaterMark.Value();

  // Step 3.
  if (std::isnan(highWaterMark) || highWaterMark < 0) {
    aRv.ThrowRangeError("Invalid highWaterMark");
    return 0.0;
  }

  // Step 4.
  return highWaterMark;
}

}  // namespace mozilla::dom
