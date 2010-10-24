/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
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

#if !defined jsjaeger_bytecodeAnalyzer_h__ && defined JS_METHODJIT
#define jsjaeger_bytecodeAnalyzer_h__

#include "jsapi.h"
#include "jscntxt.h"
#include "jsscript.h"
#include "jsopcode.h"

namespace js
{
    struct OpcodeStatus
    {
        bool   visited;         /* flag for CFG traversal */
        bool   exceptionEntry;  /* true iff this is a catch/finally entry point */
        bool   safePoint;       /* false by default */
        bool   trap;            /* It's a trap! */
        bool   inTryBlock;      /* true if in try block */
        uint32 nincoming;       /* number of CFG inedges here */
        uint32 stackDepth;      /* stack depth before this opcode */
    };

    class BytecodeAnalyzer
    {
        JSContext    *cx;
        JSScript     *script;
        OpcodeStatus *ops;
        Vector<jsbytecode *, 16, ContextAllocPolicy> doList;

        /* Whether there are POPV/SETRVAL bytecodes which can write to the frame's rval. */
        bool usesRval;

        /* Whether there are NAME bytecodes which can access the frame's scope chain. */
        bool usesScope;

      public:
        BytecodeAnalyzer(JSContext *cx, JSScript *script)
          : cx(cx), script(script), ops(NULL),
            doList(ContextAllocPolicy(cx)),
            usesRval(false), usesScope(false)
        {
        }
        ~BytecodeAnalyzer();

        bool analyze(uint32 offs);
        bool addEdge(jsbytecode *pc, int32 offset, uint32 stackDepth);

      public:

        bool usesReturnValue() const { return usesRval; }
        bool usesScopeChain() const { return usesScope; }

        inline const OpcodeStatus & operator [](uint32 offs) const {
            JS_ASSERT(offs < script->length);
            return ops[offs];
        }

        inline OpcodeStatus & operator [](uint32 offs) {
            JS_ASSERT(offs < script->length);
            return ops[offs];
        }

        inline const OpcodeStatus & operator [](jsbytecode *pc) const {
            JS_ASSERT(pc < script->code + script->length);
            return ops[pc - script->code];
        }

        inline OpcodeStatus & operator [](jsbytecode *pc) {
            JS_ASSERT(pc < script->code + script->length);
            return ops[pc - script->code];
        }

        inline bool OOM() { return !ops; }

        bool analyze();
    };
}

#endif /* jsjaeger_bytecodeAnalyzer_h__ */

