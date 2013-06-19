/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_baseline_helpers_h__
#define jsion_baseline_helpers_h__

#ifdef JS_ION

#if defined(JS_CPU_X86)
# include "x86/BaselineHelpers-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/BaselineHelpers-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/BaselineHelpers-arm.h"
#else
# error "Unknown architecture!"
#endif

namespace js {
namespace ion {

} // namespace ion
} // namespace js

#endif // JS_ION

#endif // jsion_baseline_helpers_h__
