/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineHelpers_h
#define jit_BaselineHelpers_h

#ifdef JS_ION

#if defined(JS_CPU_X86)
# include "jit/x86/BaselineHelpers-x86.h"
#elif defined(JS_CPU_X64)
# include "jit/x64/BaselineHelpers-x64.h"
#elif defined(JS_CPU_ARM)
# include "jit/arm/BaselineHelpers-arm.h"
#else
# error "Unknown architecture!"
#endif

namespace js {
namespace ion {

} // namespace ion
} // namespace js

#endif // JS_ION

#endif /* jit_BaselineHelpers_h */
