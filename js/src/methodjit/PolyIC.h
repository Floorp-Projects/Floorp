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

#if !defined jsjaeger_poly_ic_h__ && defined JS_METHODJIT
#define jsjaeger_poly_ic_h__

#include "jscntxt.h"
#include "jstl.h"
#include "jsvector.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "methodjit/MethodJIT.h"
#include "BaseAssembler.h"
#include "RematInfo.h"
#include "BaseCompiler.h"

namespace js {
namespace mjit {
namespace ic {

/* Maximum number of stubs for a given callsite. */
static const uint32 MAX_PIC_STUBS = 16;
static const uint32 MAX_GETELEM_IC_STUBS = 17;

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

    /* BindNameCompiler */
    struct {
        /* Offset from shapeGuard to end of shape jump. */
        int32 inlineJumpOffset : 8;
    } bindname;
};
#endif

enum LookupStatus {
    Lookup_Error = 0,
    Lookup_Uncacheable,
    Lookup_Cacheable
};

struct BaseIC : public MacroAssemblerTypedefs {
    BaseIC() { }

    // Address of inline fast-path.
    CodeLocationLabel fastPathStart;

    // Address to rejoin to the fast-path.
    CodeLocationLabel fastPathRejoin;

    // Start of the slow path.
    CodeLocationLabel slowPathStart;

    // Slow path stub call.
    CodeLocationCall slowPathCall;

    // Address of the start of the last generated stub, if any.
    CodeLocationLabel lastStubStart;

    // Return the start address of the last path in this PIC, which is the
    // inline path if no stubs have been generated yet.
    CodeLocationLabel lastPathStart() {
        return stubsGenerated > 0 ? lastStubStart : fastPathStart;
    }

    // Whether or not the callsite has been hit at least once.
    bool hit : 1;
    bool slowCallPatched : 1;

    // Number of stubs generated.
    uint32 stubsGenerated : 5;

    // Offset from start of stub to jump target of second shape guard as Nitro
    // asm data location. This is 0 if there is only one shape guard in the
    // last stub.
    int secondShapeGuard : 11;

    // Opcode this was compiled for.
    JSOp op : 9;

    void reset() {
        hit = false;
        slowCallPatched = false;
        stubsGenerated = 0;
        secondShapeGuard = 0;
    }
    bool shouldUpdate(JSContext *cx);
    void spew(JSContext *cx, const char *event, const char *reason);
    LookupStatus disable(JSContext *cx, const char *reason, void *stub);
    bool isCallOp();
};

struct BasePolyIC : public BaseIC {
    BasePolyIC() : execPools(SystemAllocPolicy()) { }
    ~BasePolyIC() { releasePools(); }

    void reset() {
        BaseIC::reset();
        releasePools();
        execPools.clear();
    }

    typedef Vector<JSC::ExecutablePool *, 0, SystemAllocPolicy> ExecPoolVector;

    // ExecutablePools that IC stubs were generated into.
    ExecPoolVector execPools;

    // Release ExecutablePools referred to by this PIC.
    void releasePools() {
        for (JSC::ExecutablePool **pExecPool = execPools.begin();
             pExecPool != execPools.end();
             ++pExecPool) {
            (*pExecPool)->release();
        }
    }
};

struct GetElementIC : public BasePolyIC {
    GetElementIC() { reset(); }

    // On stub entry:
    //   If hasInlineTypeCheck() is true, and inlineTypeCheckPatched is false,
    //     - typeReg contains the type of the |id| parameter.
    //   If hasInlineTypeCheck() is true, and inlineTypeCheckPatched is true,
    //     - typeReg contains the shape of |objReg| iff typeRegHasBaseShape
    //       is true.
    //   Otherwise, typeReg is garbage.
    //
    // On stub exit, typeReg must contain the type of the result value.
    RegisterID typeReg   : 5;

    // On stub entry, objReg contains the object pointer for the |obj| parameter.
    // On stub exit, objReg must contain the payload of the result value.
    RegisterID objReg    : 5;

    // Offset from the fast path to the inline type check.
    // This is only set if hasInlineTypeCheck() is true.
    unsigned inlineTypeGuard  : 8;

    // Offset from the fast path to the inline clasp guard. This is always
    // set; if |id| is known to not be int32, then it's an unconditional
    // jump to the slow path.
    unsigned inlineClaspGuard : 8;

    // This is usable if hasInlineTypeGuard() returns true, which implies
    // that a dense array fast path exists. The inline type guard serves as
    // the head of the chain of all string-based element stubs.
    bool inlineTypeGuardPatched : 1;

    // This is always usable, and specifies whether the inline clasp guard
    // has been patched. If hasInlineTypeGuard() is true, it guards against
    // a dense array, and guarantees the inline type guard has passed.
    // Otherwise, there is no inline type guard, and the clasp guard is just
    // an unconditional jump.
    bool inlineClaspGuardPatched : 1;

    ////////////////////////////////////////////
    // State for string-based property stubs. //
    ////////////////////////////////////////////

    // True if typeReg is guaranteed to have the shape of objReg.
    bool typeRegHasBaseShape : 1;

    // These offsets are used for string-key dependent stubs, such as named
    // property accesses. They are separated from the int-key dependent stubs,
    // in order to guarantee that the id type needs only one guard per type.
    int atomGuard : 8;          // optional, non-zero if present
    int firstShapeGuard : 8;    // always set
    int secondShapeGuard : 8;   // optional, non-zero if present

    bool hasLastStringStub : 1;
    CodeLocationLabel lastStringStub;

    // A limited ValueRemat instance. It may contains either:
    //  1) A constant, or
    //  2) A known type and data reg, or
    //  3) A data reg.
    // The sync bits are not set, and the type reg is never set and should not
    // be used, as it is encapsulated more accurately in |typeReg|. Also, note
    // carefully that the data reg is immutable.
    ValueRemat idRemat;

    bool hasInlineTypeGuard() const {
        return !idRemat.isTypeKnown();
    }
    bool shouldPatchInlineTypeGuard() {
        return hasInlineTypeGuard() && !inlineTypeGuardPatched;
    }
    bool shouldPatchUnconditionalClaspGuard() {
        return !hasInlineTypeGuard() && !inlineClaspGuardPatched;
    }

    void reset() {
        BasePolyIC::reset();
        inlineTypeGuardPatched = false;
        inlineClaspGuardPatched = false;
        typeRegHasBaseShape = false;
        hasLastStringStub = false;
    }
    void purge();
    LookupStatus update(JSContext *cx, JSObject *obj, const Value &v, jsid id, Value *vp);
    LookupStatus attachGetProp(JSContext *cx, JSObject *obj, const Value &v, jsid id,
                               Value *vp);
    LookupStatus disable(JSContext *cx, const char *reason);
    LookupStatus error(JSContext *cx);
    bool shouldUpdate(JSContext *cx);
};

struct SetElementIC : public BaseIC {
    SetElementIC() : execPool(NULL) { reset(); }
    ~SetElementIC() {
        if (execPool)
            execPool->release();
    }

    // On stub entry:
    //   objReg contains the payload of the |obj| parameter.
    // On stub exit:
    //   objReg may be clobbered.
    RegisterID objReg    : 5;

    // Information on how to rematerialize |objReg|.
    int32 objRemat       : MIN_STATE_REMAT_BITS;

    // Offset from the start of the fast path to the inline clasp guard.
    unsigned inlineClaspGuard : 6;

    // True if the clasp guard has been patched; false otherwise.
    bool inlineClaspGuardPatched : 1;

    // Offset from the start of the fast path to the inline hole guard.
    unsigned inlineHoleGuard : 8;

    // True if the capacity guard has been patched; false otherwise.
    bool inlineHoleGuardPatched : 1;

    // True if this is from a strict-mode script.
    bool strictMode : 1;

    // If true, then keyValue contains a constant index value >= 0. Otherwise,
    // keyReg contains a dynamic integer index in any range.
    bool hasConstantKey : 1;
    union {
        RegisterID keyReg;
        int32      keyValue;
    };

    // Rematerialize information about the value being stored.
    ValueRemat vr;

    // Optional executable pool for the out-of-line hole stub.
    JSC::ExecutablePool *execPool;

    void reset() {
        BaseIC::reset();
        if (execPool != NULL)
            execPool->release();
        execPool = NULL;
        inlineClaspGuardPatched = false;
        inlineHoleGuardPatched = false;
    }
    void purge();
    LookupStatus attachHoleStub(JSContext *cx, JSObject *obj, int32 key);
    LookupStatus update(JSContext *cx, const Value &objval, const Value &idval);
    LookupStatus disable(JSContext *cx, const char *reason);
    LookupStatus error(JSContext *cx);
};

struct PICInfo : public BasePolyIC {
    PICInfo() { reset(); }

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
        XNAME       // JSOP_GETXPROP
    };

    union {
        struct {
            RegisterID typeReg  : 5;  // reg used for checking type
            bool hasTypeCheck   : 1;  // type check and reg are present

            // Reverse offset from slowPathStart to the type check slow path.
            int32 typeCheckOffset;
        } get;
        ValueRemat vr;
    } u;

    Kind kind : 3;

    // True if register R holds the base object shape along exits from the
    // last stub.
    bool shapeRegHasBaseShape : 1;

    // True if can use the property cache.
    bool usePropCache : 1;

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
    inline bool shapeNeedsRemat() {
        return !shapeRegHasBaseShape;
    }
    inline bool isFastCall() {
        JS_ASSERT(kind == CALL);
        return !hasTypeCheck();
    }

#if defined JS_CPU_X64
    // Required labels for platform-specific patching.
    PICLabels labels;
#endif

    // Where in the script did we generate this PIC?
    jsbytecode *pc;
    
    // Index into the script's atom table.
    JSAtom *atom;

    // Reset the data members to the state of a fresh PIC before any patching
    // or stub generation was done.
    void reset() {
        BasePolyIC::reset();
        inlinePathPatched = false;
        shapeRegHasBaseShape = true;
    }
};

#ifdef JS_POLYIC
void PurgePICs(JSContext *cx, JSScript *script);
void JS_FASTCALL GetProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL SetProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL CallProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL Name(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL XName(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL BindName(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL GetElement(VMFrame &f, ic::GetElementIC *);
void JS_FASTCALL CallElement(VMFrame &f, ic::GetElementIC *);
template <JSBool strict> void JS_FASTCALL SetElement(VMFrame &f, ic::SetElementIC *);
#endif

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_poly_ic_h__ */

