/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_baselinecompiler_shared_h__
#define jsion_baselinecompiler_shared_h__

#include "jscntxt.h"
#include "ion/BaselineFrameInfo.h"
#include "ion/BaselineIC.h"
#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

class BaselineCompilerShared
{
  protected:
    JSContext *cx;
    JSScript *script;
    jsbytecode *pc;
    MacroAssembler masm;

    FrameInfo frame;
    js::Vector<CacheData, 16, SystemAllocPolicy> caches_;

    BaselineCompilerShared(JSContext *cx, JSScript *script);

    bool allocateCache(const BaseCache &cache) {
        JS_ASSERT(cache.data.pc == pc);
        return caches_.append(cache.data);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_baselinecompiler_shared_h__
