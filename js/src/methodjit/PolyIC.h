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

/* Maximum number of stubs for a given callsite. */
static const uint32 MAX_PIC_STUBS = 16;

/* SetPropCompiler */
#if defined JS_CPU_X86
static const int32 SETPROP_INLINE_SHAPE_OFFSET     =   6; //asserted
static const int32 SETPROP_INLINE_SHAPE_JUMP       =  12; //asserted
static const int32 SETPROP_DSLOTS_BEFORE_CONSTANT  = -23; //asserted
static const int32 SETPROP_DSLOTS_BEFORE_KTYPE     = -19; //asserted
static const int32 SETPROP_DSLOTS_BEFORE_DYNAMIC   = -15; //asserted
static const int32 SETPROP_INLINE_STORE_DYN_TYPE   =  -6; //asserted
static const int32 SETPROP_INLINE_STORE_DYN_DATA   =   0; //asserted
static const int32 SETPROP_INLINE_STORE_KTYPE_TYPE = -10; //asserted
static const int32 SETPROP_INLINE_STORE_KTYPE_DATA =   0; //asserted
static const int32 SETPROP_INLINE_STORE_CONST_TYPE = -14; //asserted
static const int32 SETPROP_INLINE_STORE_CONST_DATA =  -4; //asserted
static const int32 SETPROP_STUB_SHAPE_JUMP         =  12; //asserted
#elif defined JS_CPU_X64
static const int32 SETPROP_INLINE_STORE_VALUE      =   0; //asserted
static const int32 SETPROP_INLINE_SHAPE_JUMP       =   6; //asserted
#endif

/* GetPropCompiler */
#if defined JS_CPU_X86
static const int32 GETPROP_DSLOTS_LOAD         = -15; //asserted
static const int32 GETPROP_TYPE_LOAD           =  -6; //asserted
static const int32 GETPROP_DATA_LOAD           =   0; //asserted
static const int32 GETPROP_INLINE_TYPE_GUARD   =  12; //asserted
static const int32 GETPROP_INLINE_SHAPE_OFFSET =   6; //asserted
static const int32 GETPROP_INLINE_SHAPE_JUMP   =  12; //asserted
static const int32 GETPROP_STUB_SHAPE_JUMP     =  12; //asserted
#elif defined JS_CPU_X64
static const int32 GETPROP_INLINE_TYPE_GUARD   =  19; //asserted
static const int32 GETPROP_INLINE_SHAPE_JUMP   =   6; //asserted
#endif

/* GetElemCompiler */
#if defined JS_CPU_X86
static const int32 GETELEM_DSLOTS_LOAD         = -15; //asserted
static const int32 GETELEM_TYPE_LOAD           =  -6; //asserted
static const int32 GETELEM_DATA_LOAD           =   0; //asserted
static const int32 GETELEM_INLINE_SHAPE_OFFSET =   6; //asserted
static const int32 GETELEM_INLINE_SHAPE_JUMP   =  12; //asserted
static const int32 GETELEM_INLINE_ATOM_OFFSET  =  18; //asserted
static const int32 GETELEM_INLINE_ATOM_JUMP    =  24; //asserted
static const int32 GETELEM_STUB_ATOM_JUMP      =  12; //asserted
static const int32 GETELEM_STUB_SHAPE_JUMP     =  24; //asserted
#elif defined JS_CPU_X64
static const int32 GETELEM_INLINE_SHAPE_JUMP   =   6; //asserted
static const int32 GETELEM_INLINE_ATOM_JUMP    =   9; //asserted
static const int32 GETELEM_STUB_ATOM_JUMP      =  19; //asserted (probably differs)
#endif

/* ScopeNameCompiler */
#if defined JS_CPU_X86
static const int32 SCOPENAME_JUMP_OFFSET = 5; //asserted
#elif defined JS_CPU_X64
static const int32 SCOPENAME_JUMP_OFFSET = 5; //asserted
#endif

/* BindNameCompiler */
#if defined JS_CPU_X86
static const int32 BINDNAME_INLINE_JUMP_OFFSET = 10; //asserted
static const int32 BINDNAME_STUB_JUMP_OFFSET   =  5; //asserted
#elif defined JS_CPU_X64
static const int32 BINDNAME_STUB_JUMP_OFFSET   =  5; //asserted
#endif

void PurgePICs(JSContext *cx);

/*
 * x86_64 bytecode differs in length based on the involved registers.
 * Since constants won't work, we need an array of labels.
 */
#if defined JS_CPU_X64
union PICLabels {
    /* SetPropCompiler */
    struct {
        /* Offset from storeBack to beginning of 'mov dslots, addr' */
        int32 dslotsLoadOffset : 8;

        /* Offset from shapeGuard to end of shape comparison. */
        int32 inlineShapeOffset : 8;

        /* Offset from lastStubStart to end of shape jump. */
        // TODO: We can redefine the location of lastStubStart to be
        // after the jump -- at which point this is always 0.
        int32 stubShapeJump : 8;
    } setprop;

    /* GetPropCompiler */
    struct {
        /* Offset from storeBack to beginning of 'mov dslots, addr' */
        int32 dslotsLoadOffset : 8;

        /* Offset from shapeGuard to end of shape comparison. */
        int32 inlineShapeOffset : 8;
    
        /* Offset from storeBack to end of value load. */
        int32 inlineValueOffset : 8;

        /* Offset from lastStubStart to end of shape jump. */
        // TODO: We can redefine the location of lastStubStart to be
        // after the jump -- at which point this is always 0.
        int32 stubShapeJump : 8;
    } getprop;

    /* GetElemCompiler */
    struct {
        /* Offset from storeBack to beginning of 'mov dslots, addr' */
        int32 dslotsLoadOffset : 8;

        /* Offset from shapeGuard to end of shape comparison. */
        int32 inlineShapeOffset : 8;
        
        /* Offset from shapeGuard to end of atom comparison. */
        int32 inlineAtomOffset : 8;

        /* Offset from storeBack to end of value load. */
        int32 inlineValueOffset : 8;

        /* Offset from lastStubStart to end of shape jump. */
        // TODO: We can redefine the location of lastStubStart to be
        // after the jump -- at which point this is always 0.
        int32 stubShapeJump : 8;
    } getelem;

    /* BindNameCompiler */
    struct {
        /* Offset from shapeGuard to end of shape jump. */
        int32 inlineJumpOffset : 8;
    } bindname;
};
#endif

struct BaseIC {
    // Address of inline fast-path.
    JSC::CodeLocationLabel fastPathStart;

    // Address to rejoin to the fast-path.
    JSC::CodeLocationLabel fastPathRejoin;

    // Start of the slow path.
    JSC::CodeLocationLabel slowPathStart;

    // Slow path stub call.
    JSC::CodeLocationCall slowPathCall;

    // Address of the start of the last generated stub, if any.
    JSC::CodeLocationLabel lastStubStart;

    typedef Vector<JSC::ExecutablePool *, 0, SystemAllocPolicy> ExecPoolVector;

    // ExecutablePools that IC stubs were generated into.
    ExecPoolVector execPools;

    // Return the start address of the last path in this PIC, which is the
    // inline path if no stubs have been generated yet.
    JSC::CodeLocationLabel lastPathStart() {
        return stubsGenerated > 0 ? lastStubStart : fastPathStart;
    }

    // Whether or not the callsite has been hit at least once.
    bool hit : 1;

    // Number of stubs generated.
    uint32 stubsGenerated : 5;

    // Release ExecutablePools referred to by this PIC.
    void releasePools() {
        for (JSC::ExecutablePool **pExecPool = execPools.begin();
             pExecPool != execPools.end();
             ++pExecPool) {
            (*pExecPool)->release();
        }
    }

    void reset() {
        hit = false;
        stubsGenerated = 0;
        releasePools();
        execPools.clear();
    }
};

struct PICInfo : public BaseIC {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    // Operation this is a PIC for.
    enum Kind
#ifdef _MSC_VER
    : uint8_t
#endif
    {
        GET,        // JSOP_GETPROP
        CALL,       // JSOP_CALLPROP
        SET,        // JSOP_SETPROP, JSOP_SETNAME
        SETMETHOD,  // JSOP_SETMETHOD
        NAME,       // JSOP_NAME
        BIND,       // JSOP_BINDNAME
        GETELEM,    // JSOP_GETELEM
        XNAME       // JSOP_GETXPROP
    };

    union {
        struct {
            RegisterID typeReg  : 5;  // reg used for checking type
            bool hasTypeCheck   : 1;  // type check and reg are present

            // Reverse offset from slowPathStart to the type check slow path.
            int32 typeCheckOffset;

            // Remat info for the object reg.
            int32 objRemat      : MIN_STATE_REMAT_BITS;
            bool objNeedsRemat  : 1;
            RegisterID idReg    : 5;  // only used in GETELEM PICs.
        } get;
        ValueRemat vr;
    } u;

    // Offset from start of stub to jump target of second shape guard as Nitro
    // asm data location. This is 0 if there is only one shape guard in the
    // last stub.
    int secondShapeGuard : 11;

    Kind kind : 3;

    // True if register R holds the base object shape along exits from the
    // last stub.
    bool shapeRegHasBaseShape : 1;

    // State flags.
    bool inlinePathPatched : 1;     // inline path has been patched

    RegisterID shapeReg : 5;        // also the out type reg
    RegisterID objReg   : 5;        // also the out data reg

    // Offset from start of fast path to initial shape guard.
    uint32 shapeGuard;
    
    inline bool isSet() const {
        return kind == SET || kind == SETMETHOD;
    }
    inline bool isGet() const {
        return kind == GET || kind == CALL || kind == GETELEM;
    }
    inline RegisterID typeReg() {
        JS_ASSERT(isGet());
        return u.get.typeReg;
    }
    inline bool hasTypeCheck() {
        JS_ASSERT(isGet());
        return u.get.hasTypeCheck;
    }
    inline const StateRemat objRemat() const {
        JS_ASSERT(isGet());
        return StateRemat::FromInt32(u.get.objRemat);
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

    inline void setObjRemat(const StateRemat &sr) {
        JS_ASSERT(isGet());
        u.get.objRemat = sr.toInt32();
        JS_ASSERT(u.get.objRemat == sr.toInt32());
    }

#if defined JS_CPU_X64
    // Required labels for platform-specific patching.
    PICLabels labels;
#endif

    // Index into the script's atom table.
    JSAtom *atom;

    bool shouldGenerate() {
        return stubsGenerated < MAX_PIC_STUBS || !inlinePathPatched;
    }

    // Reset the data members to the state of a fresh PIC before any patching
    // or stub generation was done.
    void reset() {
        inlinePathPatched = false;
        if (kind == GET || kind == CALL || kind == GETELEM)
            u.get.objNeedsRemat = false;
        secondShapeGuard = 0;
        shapeRegHasBaseShape = true;
        BaseIC::reset();
    }
};

void PurgePICs(JSContext *cx, JSScript *script);
void JS_FASTCALL GetProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL GetElem(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL SetProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL CallProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL Name(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL XName(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL BindName(VMFrame &f, ic::PICInfo *);

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_poly_ic_h__ */

