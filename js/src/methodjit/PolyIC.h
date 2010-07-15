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
 *   David Mandelin <dmandelin@mozilla.com>
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

#if !defined jsjaeger_poly_ic_h__ && defined JS_METHODJIT && defined JS_POLYIC
#define jsjaeger_poly_ic_h__

#include "jscntxt.h"
#include "jstl.h"
#include "jsvector.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "methodjit/MethodJIT.h"
#include "RematInfo.h"

namespace js {
namespace mjit {
namespace ic {

static const uint32 MAX_PIC_STUBS = 16;

void PurgePICs(JSContext *cx);

struct PICInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    // Operation this is a PIC for.
    enum Kind
#ifdef _MSC_VER
    : uint8_t
#endif
    {
        GET,
        CALL,
        SET,
        NAME,
        BIND
    };

    union {
        // This struct comes out to 61 bits.
        struct {
            RegisterID typeReg  : 5;  // reg used for checking type
            bool hasTypeCheck   : 1;  // type check and reg are present

            // Reverse offset from slowPathStart to the type check slow path.
            uint16 typeCheckOffset : 9;

            // Remat info for the object reg.
            uint32 objRemat    : 20;
            bool objNeedsRemat : 1;

            // Offset from start of stub to jump target of second shape guard as Nitro
            // asm data location. This is 0 if there is only one shape guard in the
            // last stub.
            int secondShapeGuard : 8;
        } get;
        ValueRemat vr;
    } u;

    Kind kind : 3;

    // True if register R holds the base object shape along exits from the
    // last stub.
    bool shapeRegHasBaseShape : 1;

    // State flags.
    bool hit : 1;                   // this PIC has been executed
    bool inlinePathPatched : 1;     // inline path has been patched

    RegisterID shapeReg : 5;        // also the out type reg
    RegisterID objReg   : 5;        // also the out data reg

    inline bool isGet() {
        return kind == GET || kind == CALL;
    }
    inline RegisterID typeReg() {
        JS_ASSERT(isGet());
        return u.get.typeReg;
    }
    inline bool hasTypeCheck() {
        JS_ASSERT(isGet());
        return u.get.hasTypeCheck;
    }
    inline uint32 objRemat() {
        JS_ASSERT(isGet());
        return u.get.objRemat;
    }
    inline bool objNeedsRemat() {
        JS_ASSERT(isGet());
        return u.get.objNeedsRemat;
    }
    inline bool shapeNeedsRemat() {
        return !shapeRegHasBaseShape;
    }
    inline bool isFastCall() {
        JS_ASSERT(kind == CALL);
        return !hasTypeCheck();
    }

    // Number of stubs generated.
    uint32 stubsGenerated : 5;

    // Offset from start of fast path to initial shape guard.
    int shapeGuard : 8;
    
    // Index into the script's atom table.
    JSAtom *atom;

    // Address of inline fast-path.
    JSC::CodeLocationLabel fastPathStart;

    // Address of store back at the end of the inline fast-path.
    JSC::CodeLocationLabel storeBack;

    // Return address of slow path call, as an offset from slowPathStart.
    uint16 callReturn : 9;

    // Offset from callReturn to the start of the slow case.
    JSC::CodeLocationLabel slowPathStart;

    // Address of the start of the last generated stub, if any.
    JSC::CodeLocationLabel lastStubStart;

    typedef Vector<JSC::ExecutablePool *, 0, SystemAllocPolicy> ExecPoolVector;

    // ExecutablePools that PIC stubs were generated into.
    ExecPoolVector execPools;

    // Return the start address of the last path in this PIC, which is the
    // inline path if no stubs have been generated yet.
    JSC::CodeLocationLabel lastPathStart() {
        return stubsGenerated > 0 ? lastStubStart : fastPathStart;
    }

    bool shouldGenerate() {
        return stubsGenerated < MAX_PIC_STUBS || !inlinePathPatched;
    }

    // Release ExecutablePools referred to by this PIC.
    void releasePools() {
        for (JSC::ExecutablePool **pExecPool = execPools.begin();
             pExecPool != execPools.end();
             ++pExecPool)
        {
            (*pExecPool)->release();
        }
    }

    // Reset the data members to the state of a fresh PIC before any patching
    // or stub generation was done.
    void reset() {
        hit = false;
        inlinePathPatched = false;
        if (kind == GET) {
            u.get.secondShapeGuard = 0;
            u.get.objNeedsRemat = false;
        }
        shapeRegHasBaseShape = true;
        stubsGenerated = 0;
        releasePools();
        execPools.clear();
    }
};

void PurgePICs(JSContext *cx, JSScript *script);
void JS_FASTCALL GetProp(VMFrame &f, uint32 index);
void JS_FASTCALL SetProp(VMFrame &f, uint32 index);
void JS_FASTCALL CallProp(VMFrame &f, uint32 index);
void JS_FASTCALL Name(VMFrame &f, uint32 index);
void JS_FASTCALL BindName(VMFrame &f, uint32 index);

}
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_poly_ic_h__ */

