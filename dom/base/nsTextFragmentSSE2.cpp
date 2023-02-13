/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextFragmentGeneric.h"

namespace mozilla {
template int32_t FirstNon8Bit<xsimd::sse2>(const char16_t*, const char16_t*);
}  // namespace mozilla
