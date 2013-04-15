/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_baselinecompiler_x86_shared_h__
#define jsion_baselinecompiler_x86_shared_h__

#include "ion/shared/BaselineCompiler-shared.h"

namespace js {
namespace ion {

class BaselineCompilerX86Shared : public BaselineCompilerShared
{
  protected:
    BaselineCompilerX86Shared(JSContext *cx, HandleScript script);
};

} // namespace ion
} // namespace js

#endif // jsion_baselinecompiler_x86_shared_h__
