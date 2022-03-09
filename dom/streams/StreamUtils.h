/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StreamUtils_h
#define mozilla_dom_StreamUtils_h

#include "mozilla/ErrorResult.h"

namespace mozilla::dom {

struct QueuingStrategy;

double ExtractHighWaterMark(const QueuingStrategy& aStrategy,
                            double aDefaultHWM, ErrorResult& aRv);

}  // namespace mozilla::dom

#endif  // mozilla_dom_StreamUtils_h
