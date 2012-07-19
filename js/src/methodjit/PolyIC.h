/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_poly_ic_h__ && defined JS_METHODJIT
#define jsjaeger_poly_ic_h__

#include "jscntxt.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "js/Vector.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/ICRepatcher.h"
#include "BaseAssembler.h"
#include "RematInfo.h"
#include "BaseCompiler.h"
#include "methodjit/ICLabels.h"
#include "assembler/moco/MocoStubs.h"

namespace js {
namespace mjit {
namespace ic {

/* Maximum number of stubs for a given callsite. */
static const uint32_t MAX_PIC_STUBS = 16;
static const uint32_t MAX_GETELEM_IC_STUBS = 17;

enum LookupStatus {
    Lookup_Error = 0,
    Lookup_Uncacheable,
    Lookup_Cacheable
};

struct BaseIC : public MacroAssemblerTypedefs {

    // Address of inline fast-path.
    CodeLocationLabel fastPathStart;

    // Address to rejoin to the fast-path.
    CodeLocationLabel fastPathRejoin;

    // Start of the slow path.
    CodeLocationLabel slowPathStart;

    // Slow path stub call.
    CodeLocationCall slowPathCall;

    // Offset from start of stub to jump target of second shape guard as Nitro
    // asm data location. This is 0 if there is only one shape guard in the
    // last stub.
    int32_t secondShapeGuard;

    // Whether or not the callsite has been hit at least once.
    bool hit : 1;
    bool slowCallPatched : 1;

    // Whether getter/setter hooks can be called from IC stubs.
    bool canCallHook : 1;

    // Whether a type barrier is in place for the result of the op.
    bool forcedTypeBarrier : 1;

    // Number of stubs generated.
    uint32_t stubsGenerated : 5;

    bool shouldUpdate(VMFrame &f);
    void spew(VMFrame &f, const char *event, const char *reason);
    LookupStatus disable(VMFrame &f, const char *reason, void *stub);
    void updatePCCounters(VMFrame &f, Assembler &masm);

  protected:
    void reset() {
        hit = false;
        slowCallPatched = false;
        forcedTypeBarrier = false;
        stubsGenerated = 0;
        secondShapeGuard = 0;
    }
};

class BasePolyIC : public BaseIC {
    typedef Vector<JSC::ExecutablePool *, 2, SystemAllocPolicy> ExecPoolVector;

    // ExecutablePools that IC stubs were generated into.  Very commonly (eg.
    // 99.5% of BasePolyICs) there are 0 or 1, and there are lots of
    // BasePolyICs, so we space-optimize for that case.  If the bottom bit of
    // the pointer is 0, execPool should be used, and it will be NULL (for 0
    // pools) or non-NULL (for 1 pool).  If the bottom bit of the
    // pointer is 1, taggedExecPools should be used, but only after de-tagging
    // (for 2 or more pools).
    union {
        JSC::ExecutablePool *execPool;      // valid when bottom bit is a 0
        ExecPoolVector *taggedExecPools;    // valid when bottom bit is a 1
    } u;

    static bool isTagged(void *p) {
        return !!(intptr_t(p) & 1);
    }

    static ExecPoolVector *tag(ExecPoolVector *p) {
        JS_ASSERT(!isTagged(p));
        return (ExecPoolVector *)(intptr_t(p) | 1);
    }

    static ExecPoolVector *detag(ExecPoolVector *p) {
        JS_ASSERT(isTagged(p));
        return (ExecPoolVector *)(intptr_t(p) & ~1);
    }

    bool areZeroPools()     { return !u.execPool; }
    bool isOnePool()        { return u.execPool && !isTagged(u.execPool); }
    bool areMultiplePools() { return isTagged(u.taggedExecPools); }

    ExecPoolVector *multiplePools() {
        JS_ASSERT(areMultiplePools());
        return detag(u.taggedExecPools);
    }

  public:
    bool addPool(JSContext *cx, JSC::ExecutablePool *pool) {
        if (areZeroPools()) {
            u.execPool = pool;
            return true;
        }
        if (isOnePool()) {
            JSC::ExecutablePool *oldPool = u.execPool;
            JS_ASSERT(!isTagged(oldPool));
            ExecPoolVector *execPools = OffTheBooks::new_<ExecPoolVector>(SystemAllocPolicy());
            if (!execPools)
                return false;
            if (!execPools->append(oldPool) || !execPools->append(pool)) {
                Foreground::delete_(execPools);
                return false;
            }
            u.taggedExecPools = tag(execPools);
            return true;
        }
        return multiplePools()->append(pool);
    }

  protected:
    void reset() {
        BaseIC::reset();
        if (areZeroPools()) {
            // Common case:  do nothing.
        } else if (isOnePool()) {
            u.execPool->release();
            u.execPool = NULL;
        } else {
            ExecPoolVector *execPools = multiplePools();
            for (size_t i = 0; i < execPools->length(); i++)
                (*execPools)[i]->release();
            Foreground::delete_(execPools);
            u.execPool = NULL;
        }
        JS_ASSERT(areZeroPools());
    }
};

struct GetElementIC : public BasePolyIC {

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

    // Offset from the fast path to the inline shape guard. This is always
    // set; if |id| is known to not be int32, then it's an unconditional
    // jump to the slow path.
    unsigned inlineShapeGuard : 8;

    // This is usable if hasInlineTypeGuard() returns true, which implies
    // that a dense array fast path exists. The inline type guard serves as
    // the head of the chain of all string-based element stubs.
    bool inlineTypeGuardPatched : 1;

    // This is always usable, and specifies whether the inline shape guard
    // has been patched. If hasInlineTypeGuard() is true, it guards against
    // a dense array, and guarantees the inline type guard has passed.
    // Otherwise, there is no inline type guard, and the shape guard is just
    // an unconditional jump.
    bool inlineShapeGuardPatched : 1;

    ////////////////////////////////////////////
    // State for string-based property stubs. //
    ////////////////////////////////////////////

    // True if typeReg is guaranteed to have the shape of objReg.
    bool typeRegHasBaseShape : 1;

    // These offsets are used for string-key dependent stubs, such as named
    // property accesses. They are separated from the int-key dependent stubs,
    // in order to guarantee that the id type needs only one guard per type.
    int32_t atomGuard : 8;          // optional, non-zero if present
    int32_t firstShapeGuard : 11;    // always set
    int32_t secondShapeGuard : 11;   // optional, non-zero if present

    bool hasLastStringStub : 1;
    JITCode lastStringStub;

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
    bool shouldPatchUnconditionalShapeGuard() {
        // The shape guard is only unconditional if the type is known to not
        // be an int32.
        if (idRemat.isTypeKnown() && idRemat.knownType() != JSVAL_TYPE_INT32)
            return !inlineShapeGuardPatched;
        return false;
    }

    void purge(Repatcher &repatcher);
    LookupStatus update(VMFrame &f, HandleObject obj, HandleValue v, HandleId id, Value *vp);
    LookupStatus attachGetProp(VMFrame &f, HandleObject obj, HandleValue v, HandlePropertyName name,
                               Value *vp);
    LookupStatus attachTypedArray(VMFrame &f, HandleObject obj, HandleValue v, HandleId id, Value *vp);
    LookupStatus disable(VMFrame &f, const char *reason);
    LookupStatus error(JSContext *cx);
    bool shouldUpdate(VMFrame &f);

  protected:
    void reset() {
        BasePolyIC::reset();
        inlineTypeGuardPatched = false;
        inlineShapeGuardPatched = false;
        typeRegHasBaseShape = false;
        hasLastStringStub = false;
    }
};

struct SetElementIC : public BaseIC {

    // On stub entry:
    //   objReg contains the payload of the |obj| parameter.
    // On stub exit:
    //   objReg may be clobbered.
    RegisterID objReg    : 5;

    // Information on how to rematerialize |objReg|.
    int32_t objRemat       : MIN_STATE_REMAT_BITS;

    // Offset from the start of the fast path to the inline shape guard.
    unsigned inlineShapeGuard : 6;

    // True if the shape guard has been patched; false otherwise.
    bool inlineShapeGuardPatched : 1;

    // Offset from the start of the fast path to the inline hole guard.
    unsigned inlineHoleGuard : 8;

    // True if the capacity guard has been patched; false otherwise.
    bool inlineHoleGuardPatched : 1;

    // True if this is from a strict-mode script.
    bool strictMode : 1;

    // A bitmask of registers that are volatile and must be preserved across
    // stub calls inside the IC.
    uint32_t volatileMask;

    // If true, then keyValue contains a constant index value >= 0. Otherwise,
    // keyReg contains a dynamic integer index in any range.
    bool hasConstantKey : 1;
    union {
        RegisterID keyReg;
        int32_t    keyValue;
    };

    // Rematerialize information about the value being stored.
    ValueRemat vr;

    // Optional executable pool for the out-of-line hole stub.
    JSC::ExecutablePool *execPool;

    void purge(Repatcher &repatcher);
    LookupStatus attachTypedArray(VMFrame &f, JSObject *obj, int32_t key);
    LookupStatus attachHoleStub(VMFrame &f, JSObject *obj, int32_t key);
    LookupStatus update(VMFrame &f, const Value &objval, const Value &idval);
    LookupStatus disable(VMFrame &f, const char *reason);
    LookupStatus error(JSContext *cx);
    bool shouldUpdate(VMFrame &f);

  protected:
    void reset() {
        BaseIC::reset();
        if (execPool) {
            execPool->release();
            execPool = NULL;
        }
        inlineShapeGuardPatched = false;
        inlineHoleGuardPatched = false;
    }
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
        SET,        // JSOP_SETPROP, JSOP_SETNAME
        NAME,       // JSOP_NAME
        BIND,       // JSOP_BINDNAME
        XNAME       // JSOP_GETXPROP
    };

    union {
        struct {
            RegisterID typeReg  : 5;  // reg used for checking type
            bool hasTypeCheck   : 1;  // type check and reg are present

            // Reverse offset from slowPathStart to the type check slow path.
            int32_t typeCheckOffset;
        } get;
        ValueRemat vr;
    } u;

    // Address of the start of the last generated stub, if any. Note that this
    // does not correctly overlay with the allocated memory; it does however
    // overlay the portion that may need to be patched, which is good enough.
    JITCode lastStubStart;

    // Return the start address of the last path in this PIC, which is the
    // inline path if no stubs have been generated yet.
    CodeLocationLabel lastPathStart() {
        if (!stubsGenerated)
            return fastPathStart;
        return CodeLocationLabel(lastStubStart.start());
    }

    CodeLocationLabel getFastShapeGuard() {
        return fastPathStart.labelAtOffset(shapeGuard);
    }

    CodeLocationLabel getSlowTypeCheck() {
        JS_ASSERT(isGet());
        return slowPathStart.labelAtOffset(u.get.typeCheckOffset);
    }

    // Return a JITCode block corresponding to the code memory to attach a
    // new stub to.
    JITCode lastCodeBlock(JITChunk *chunk) {
        if (!stubsGenerated)
            return JITCode(chunk->code.m_code.executableAddress(), chunk->code.m_size);
        return lastStubStart;
    }

    void updateLastPath(LinkerHelper &linker, Label label) {
        CodeLocationLabel loc = linker.locationOf(label);
        lastStubStart = JITCode(loc.executableAddress(), linker.size());
    }

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

    // Whether type properties need to be updated to reflect generated stubs.
    bool typeMonitored : 1;

    // For GET caches, whether the access may use the property cache.
    bool cached : 1;

    // Offset from start of fast path to initial shape guard.
    uint32_t shapeGuard;

    // Possible types of the RHS, for monitored SETPROP PICs.
    types::TypeSet *rhsTypes;

    inline bool isSet() const {
        return kind == SET;
    }
    inline bool isGet() const {
        return kind == GET;
    }
    inline bool isBind() const {
        return kind == BIND;
    }
    inline bool isScopeName() const {
        return kind == NAME || kind == XNAME;
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

    union {
        GetPropLabels getPropLabels_;
        SetPropLabels setPropLabels_;
        BindNameLabels bindNameLabels_;
        ScopeNameLabels scopeNameLabels_;
    };
    void setLabels(const ic::GetPropLabels &labels) {
        JS_ASSERT(isGet());
        getPropLabels_ = labels;
    }
    void setLabels(const ic::SetPropLabels &labels) {
        JS_ASSERT(isSet());
        setPropLabels_ = labels;
    }
    void setLabels(const ic::BindNameLabels &labels) {
        JS_ASSERT(kind == BIND);
        bindNameLabels_ = labels;
    }
    void setLabels(const ic::ScopeNameLabels &labels) {
        JS_ASSERT(kind == NAME || kind == XNAME);
        scopeNameLabels_ = labels;
    }

    GetPropLabels &getPropLabels() {
        JS_ASSERT(isGet());
        return getPropLabels_;
    }
    SetPropLabels &setPropLabels() {
        JS_ASSERT(isSet());
        return setPropLabels_;
    }
    BindNameLabels &bindNameLabels() {
        JS_ASSERT(kind == BIND);
        return bindNameLabels_;
    }
    ScopeNameLabels &scopeNameLabels() {
        JS_ASSERT(kind == NAME || kind == XNAME);
        return scopeNameLabels_;
    }

    // Where in the script did we generate this PIC?
    jsbytecode *pc;

    // Index into the script's atom table.
    PropertyName *name;

  public:
    void purge(Repatcher &repatcher);

  protected:
    // Reset the data members to the state of a fresh PIC before any patching
    // or stub generation was done.
    void reset() {
        BasePolyIC::reset();
        inlinePathPatched = false;
        shapeRegHasBaseShape = true;
    }
};

#ifdef JS_POLYIC
void JS_FASTCALL GetProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL SetProp(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL Name(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL XName(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL BindName(VMFrame &f, ic::PICInfo *);
void JS_FASTCALL GetElement(VMFrame &f, ic::GetElementIC *);
template <JSBool strict> void JS_FASTCALL SetElement(VMFrame &f, ic::SetElementIC *);
#endif

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_poly_ic_h__ */

