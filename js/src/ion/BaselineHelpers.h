/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_helpers_h__) && defined(JS_ION)
#define jsion_baseline_helpers_h__

#if defined(JS_CPU_X86)
# include "x86/BaselineHelpers-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/BaselineHelpers-x64.h"
#else
# include "arm/BaselineHelpers-arm.h"
#endif

namespace js {
namespace ion {

} // namespace ion
} // namespace js

#endif

