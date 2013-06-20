/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_x64_BaselineCompiler_x64_h
#define ion_x64_BaselineCompiler_x64_h

#include "ion/shared/BaselineCompiler-x86-shared.h"

namespace js {
namespace ion {

class BaselineCompilerX64 : public BaselineCompilerX86Shared
{
  protected:
    BaselineCompilerX64(JSContext *cx, HandleScript script);
};

typedef BaselineCompilerX64 BaselineCompilerSpecific;

} // namespace ion
} // namespace js

#endif /* ion_x64_BaselineCompiler_x64_h */
