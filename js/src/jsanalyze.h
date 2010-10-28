/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SpiderMonkey bytecode analysis
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Definitions for javascript analysis. */

#ifndef jsanalyze_h___
#define jsanalyze_h___

#include "jsarena.h"
#include "jscntxt.h"
#include "jsscript.h"

struct JSScript;

namespace js {
namespace analyze {

class Script;

/* Information about a bytecode instruction. */
struct Bytecode
{
    friend class Script;

    /* Whether there are any incoming jumps to this instruction. */
    bool jumpTarget : 1;

    /* Whether this instruction has been analyzed to get its output defines and stack. */
    bool analyzed : 1;

    /* Whether this is a catch/finally entry point. */
    bool exceptionEntry : 1;

    /* Whether this is in a try block. */
    bool inTryBlock : 1;

    /* Whether this is a method JIT safe point. */
    bool safePoint : 1;

    /* Stack depth before this opcode. */
    uint32 stackDepth;

    /*
     * The set of locals defined at this point.  This does not include locals which
     * were unconditionally defined at an earlier point in the script.
     */
    uint32 defineCount;
    uint32 *defineArray;

    Bytecode()
    {
        PodZero(this);
    }

  private:
    bool mergeDefines(JSContext *cx,
                      Script *script, bool initial, uint32 newDepth,
                      uint32 *newArray, uint32 newCount);

    /* Whether a local variable is in the define set at this bytecode. */
    bool isDefined(uint32 slot)
    {
        JS_ASSERT(analyzed);
        for (size_t ind = 0; ind < defineCount; ind++) {
            if (defineArray[ind] == slot)
                return true;
        }
        return false;
    }
};

/* Information about a script. */
class Script
{
    friend struct Bytecode;

    JSScript *script;
    Bytecode **code;

    /* Maximum number of locals to consider for a script. */
    static const unsigned LOCAL_LIMIT = 50;

    /* Offsets at which each local becomes unconditionally defined, or a value below. */
    uint32 *locals;

    static const uint32 LOCAL_USE_BEFORE_DEF = uint32(-1);
    static const uint32 LOCAL_CONDITIONALLY_DEFINED = uint32(-2);

    bool outOfMemory;
    bool hadFailure;
    bool usesRval;
    bool usesScope;

  public:
    /* Pool for allocating analysis structures which will not outlive this script. */
    JSArenaPool pool;

    void analyze(JSContext *cx, JSScript *script);
    void destroy();

    /*
     * For analysis scripts allocated on the stack.  Scripts don't have constructors,
     * and must be zeroed out before being used.
     */
    ~Script() { destroy(); }

    /* Whether we ran out of memory during analysis. */
    bool OOM() { return outOfMemory; }

    /* Whether the script was analyzed successfully. */
    bool failed() { return hadFailure; }

    /* Whether there are POPV/SETRVAL bytecodes which can write to the frame's rval. */
    bool usesReturnValue() const { return usesRval; }

    /* Whether there are NAME bytecodes which can access the frame's scope chain. */
    bool usesScopeChain() const { return usesScope; }

    /* Accessors for bytecode information. */

    Bytecode& getCode(uint32 offset) {
        JS_ASSERT(offset < script->length);
        JS_ASSERT(code[offset]);
        return *code[offset];
    }
    Bytecode& getCode(jsbytecode *pc) { return getCode(pc - script->code); }

    Bytecode* maybeCode(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return code[offset];
    }
    Bytecode* maybeCode(jsbytecode *pc) { return maybeCode(pc - script->code); }

    bool jumpTarget(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return code[offset] && code[offset]->jumpTarget;
    }
    bool jumpTarget(jsbytecode *pc) { return jumpTarget(pc - script->code); }

    /* Accessors for local variable information. */

    unsigned localCount() {
        return (script->nfixed >= LOCAL_LIMIT) ? LOCAL_LIMIT : script->nfixed;
    }

    bool localHasUseBeforeDef(uint32 local) {
        JS_ASSERT(local < script->nfixed && !failed());
        return local >= localCount() || locals[local] == LOCAL_USE_BEFORE_DEF;
    }

    /* These return true for variables that may have a use before def. */
    bool localDefined(uint32 local, uint32 offset) {
        return localHasUseBeforeDef(local) || (locals[local] <= offset) ||
            getCode(offset).isDefined(local);
    }
    bool localDefined(uint32 local, jsbytecode *pc) {
        return localDefined(local, pc - script->code);
    }

  private:
    void setOOM(JSContext *cx) {
        if (!outOfMemory)
            js_ReportOutOfMemory(cx);
        outOfMemory = true;
        hadFailure = true;
    }

    inline bool addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump,
                        unsigned stackDepth, uint32 *defineArray, unsigned defineCount);

    inline void setLocal(uint32 local, uint32 offset);
};

} /* namespace analyze */
} /* namespace js */

#endif // jsanalyze_h___
