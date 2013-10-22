/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BytecodeAnalysis_h
#define jit_BytecodeAnalysis_h

#include "jsscript.h"
#include "jit/IonAllocPolicy.h"
#include "js/Vector.h"

namespace js {
namespace jit {

// Basic information about bytecodes in the script.  Used to help baseline compilation.
struct BytecodeInfo
{
    static const uint16_t MAX_STACK_DEPTH = 0xffffU;
    uint16_t stackDepth;
    bool initialized : 1;
    bool jumpTarget : 1;
    bool jumpFallthrough : 1;
    bool fallthrough : 1;

    // If true, this is a JSOP_LOOPENTRY op inside a catch or finally block.
    bool loopEntryInCatchOrFinally : 1;

    void init(unsigned depth) {
        JS_ASSERT(depth <= MAX_STACK_DEPTH);
        JS_ASSERT_IF(initialized, stackDepth == depth);
        initialized = true;
        stackDepth = depth;
    }
};

class BytecodeAnalysis
{
    JSScript *script_;
    Vector<BytecodeInfo, 0, IonAllocPolicy> infos_;

    bool usesScopeChain_;
    bool hasTryFinally_;
    bool hasSetArg_;

  public:
    explicit BytecodeAnalysis(JSScript *script);

    bool init(GSNCache &gsn);

    BytecodeInfo &info(jsbytecode *pc) {
        JS_ASSERT(infos_[pc - script_->code].initialized);
        return infos_[pc - script_->code];
    }

    BytecodeInfo *maybeInfo(jsbytecode *pc) {
        if (infos_[pc - script_->code].initialized)
            return &infos_[pc - script_->code];
        return nullptr;
    }

    bool usesScopeChain() const {
        return usesScopeChain_;
    }

    bool hasTryFinally() const {
        return hasTryFinally_;
    }

    bool hasSetArg() const {
        return hasSetArg_;
    }
};


} // namespace jit
} // namespace js

#endif /* jit_BytecodeAnalysis_h */
