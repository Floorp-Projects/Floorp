/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSContext_inl_h
#define vm_JSContext_inl_h

#include "vm/JSContext.h"

#include "builtin/Object.h"
#include "jit/JitFrames.h"
#include "proxy/Proxy.h"
#include "vm/HelperThreads.h"
#include "vm/Interpreter.h"
#include "vm/Iteration.h"
#include "vm/Realm.h"
#include "vm/SymbolType.h"

namespace js {

class CompartmentChecker
{
    JS::Compartment* compartment;

  public:
    explicit CompartmentChecker(JSContext* cx)
      : compartment(cx->compartment())
    {
    }

    /*
     * Set a breakpoint here (break js::CompartmentChecker::fail) to debug
     * compartment mismatches.
     */
    static void fail(JS::Compartment* c1, JS::Compartment* c2) {
        printf("*** Compartment mismatch %p vs. %p\n", (void*) c1, (void*) c2);
        MOZ_CRASH();
    }

    static void fail(JS::Zone* z1, JS::Zone* z2) {
        printf("*** Zone mismatch %p vs. %p\n", (void*) z1, (void*) z2);
        MOZ_CRASH();
    }

    static void check(JS::Compartment* c1, JS::Compartment* c2) {
        if (c1 != c2)
            fail(c1, c2);
    }

    void check(JS::Compartment* c) {
        if (c && c != compartment)
            fail(compartment, c);
    }

    void checkZone(JS::Zone* z) {
        if (compartment && z != compartment->zone())
            fail(compartment->zone(), z);
    }

    void check(JSObject* obj) {
        if (obj) {
            MOZ_ASSERT(JS::ObjectIsNotGray(obj));
            MOZ_ASSERT(!js::gc::IsAboutToBeFinalizedUnbarriered(&obj));
            check(obj->compartment());
        }
    }

    template<typename T>
    void check(const Rooted<T>& rooted) {
        check(rooted.get());
    }

    template<typename T>
    void check(Handle<T> handle) {
        check(handle.get());
    }

    template<typename T>
    void check(MutableHandle<T> handle) {
        check(handle.get());
    }

    template <typename T>
    void checkAtom(T* thing) {
        static_assert(mozilla::IsSame<T, JSAtom>::value ||
                      mozilla::IsSame<T, JS::Symbol>::value,
                      "Should only be called with JSAtom* or JS::Symbol* argument");

#ifdef DEBUG
        // Atoms which move across zone boundaries need to be marked in the new
        // zone, see JS_MarkCrossZoneId.
        if (compartment) {
            JSRuntime* rt = compartment->runtimeFromAnyThread();
            MOZ_ASSERT(rt->gc.atomMarking.atomIsMarked(compartment->zone(), thing));
        }
#endif
    }

    void check(JSString* str) {
        MOZ_ASSERT(JS::CellIsNotGray(str));
        if (str->isAtom()) {
            checkAtom(&str->asAtom());
        } else {
            checkZone(str->zone());
        }
    }

    void check(JS::Symbol* symbol) {
        checkAtom(symbol);
    }

    void check(const js::Value& v) {
        if (v.isObject())
            check(&v.toObject());
        else if (v.isString())
            check(v.toString());
        else if (v.isSymbol())
            check(v.toSymbol());
    }

    // Check the contents of any container class that supports the C++
    // iteration protocol, eg GCVector<jsid>.
    template <typename Container>
    typename mozilla::EnableIf<
        mozilla::IsSame<
            decltype(((Container*)nullptr)->begin()),
            decltype(((Container*)nullptr)->end())
        >::value
    >::Type
    check(const Container& container) {
        for (auto i : container)
            check(i);
    }

    void check(const JS::HandleValueArray& arr) {
        for (size_t i = 0; i < arr.length(); i++)
            check(arr[i]);
    }

    void check(const CallArgs& args) {
        for (Value* p = args.base(); p != args.end(); ++p)
            check(*p);
    }

    void check(jsid id) {
        if (JSID_IS_ATOM(id))
            checkAtom(JSID_TO_ATOM(id));
        else if (JSID_IS_SYMBOL(id))
            checkAtom(JSID_TO_SYMBOL(id));
        else
            MOZ_ASSERT(!JSID_IS_GCTHING(id));
    }

    void check(JSScript* script) {
        MOZ_ASSERT(JS::CellIsNotGray(script));
        if (script)
            check(script->compartment());
    }

    void check(InterpreterFrame* fp);
    void check(AbstractFramePtr frame);

    void check(Handle<PropertyDescriptor> desc) {
        check(desc.object());
        if (desc.hasGetterObject())
            check(desc.getterObject());
        if (desc.hasSetterObject())
            check(desc.setterObject());
        check(desc.value());
    }

    void check(TypeSet::Type type) {
        check(type.maybeCompartment());
    }
};

/*
 * Don't perform these checks when called from a finalizer. The checking
 * depends on other objects not having been swept yet.
 */
#define START_ASSERT_SAME_COMPARTMENT()                                 \
    if (JS::RuntimeHeapIsCollecting())                            \
        return;                                                         \
    CompartmentChecker c(cx)

template <class T1> inline void
releaseAssertSameCompartment(JSContext* cx, const T1& t1)
{
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
}

template <class T1> inline void
assertSameCompartment(JSContext* cx, const T1& t1)
{
#ifdef JS_CRASH_DIAGNOSTICS
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
#endif
}

template <class T1> inline void
assertSameCompartmentDebugOnly(JSContext* cx, const T1& t1)
{
#if defined(DEBUG) && defined(JS_CRASH_DIAGNOSTICS)
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
#endif
}

template <class T1, class T2> inline void
assertSameCompartment(JSContext* cx, const T1& t1, const T2& t2)
{
#ifdef JS_CRASH_DIAGNOSTICS
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
#endif
}

template <class T1, class T2, class T3> inline void
assertSameCompartment(JSContext* cx, const T1& t1, const T2& t2, const T3& t3)
{
#ifdef JS_CRASH_DIAGNOSTICS
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
#endif
}

template <class T1, class T2, class T3, class T4> inline void
assertSameCompartment(JSContext* cx,
                      const T1& t1, const T2& t2, const T3& t3, const T4& t4)
{
#ifdef JS_CRASH_DIAGNOSTICS
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
    c.check(t4);
#endif
}

template <class T1, class T2, class T3, class T4, class T5> inline void
assertSameCompartment(JSContext* cx,
                      const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5)
{
#ifdef JS_CRASH_DIAGNOSTICS
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
    c.check(t4);
    c.check(t5);
#endif
}

#undef START_ASSERT_SAME_COMPARTMENT

STATIC_PRECONDITION_ASSUME(ubound(args.argv_) >= argc)
MOZ_ALWAYS_INLINE bool
CallNativeImpl(JSContext* cx, NativeImpl impl, const CallArgs& args)
{
#ifdef DEBUG
    bool alreadyThrowing = cx->isExceptionPending();
#endif
    assertSameCompartment(cx, args);
    bool ok = impl(cx, args);
    if (ok) {
        assertSameCompartment(cx, args.rval());
        MOZ_ASSERT_IF(!alreadyThrowing, !cx->isExceptionPending());
    }
    return ok;
}

MOZ_ALWAYS_INLINE bool
CallJSGetterOp(JSContext* cx, GetterOp op, HandleObject obj, HandleId id,
               MutableHandleValue vp)
{
    if (!CheckRecursionLimit(cx))
        return false;

    assertSameCompartment(cx, obj, id, vp);
    bool ok = op(cx, obj, id, vp);
    if (ok)
        assertSameCompartment(cx, vp);
    return ok;
}

MOZ_ALWAYS_INLINE bool
CallJSSetterOp(JSContext* cx, SetterOp op, HandleObject obj, HandleId id, HandleValue v,
               ObjectOpResult& result)
{
    if (!CheckRecursionLimit(cx))
        return false;

    assertSameCompartment(cx, obj, id, v);
    return op(cx, obj, id, v, result);
}

inline bool
CallJSAddPropertyOp(JSContext* cx, JSAddPropertyOp op, HandleObject obj, HandleId id,
                    HandleValue v)
{
    if (!CheckRecursionLimit(cx))
        return false;

    assertSameCompartment(cx, obj, id, v);
    return op(cx, obj, id, v);
}

inline bool
CallJSDeletePropertyOp(JSContext* cx, JSDeletePropertyOp op, HandleObject receiver, HandleId id,
                       ObjectOpResult& result)
{
    if (!CheckRecursionLimit(cx))
        return false;

    assertSameCompartment(cx, receiver, id);
    if (op)
        return op(cx, receiver, id, result);
    return result.succeed();
}

MOZ_ALWAYS_INLINE bool
CheckForInterrupt(JSContext* cx)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    // Add an inline fast-path since we have to check for interrupts in some hot
    // C++ loops of library builtins.
    if (MOZ_UNLIKELY(cx->hasAnyPendingInterrupt()))
        return cx->handleInterrupt();

    JS_INTERRUPT_POSSIBLY_FAIL();

    return true;
}

}  /* namespace js */

inline js::LifoAlloc&
JSContext::typeLifoAlloc()
{
    return zone()->types.typeLifoAlloc();
}

inline js::Nursery&
JSContext::nursery()
{
    return runtime()->gc.nursery();
}

inline void
JSContext::minorGC(JS::gcreason::Reason reason)
{
    runtime()->gc.minorGC(reason);
}

inline void
JSContext::setPendingException(JS::HandleValue v)
{
#if defined(NIGHTLY_BUILD)
    do {
        // Do not intercept exceptions if we are already
        // in the exception interceptor. That would lead
        // to infinite recursion.
        if (this->runtime()->errorInterception.isExecuting)
            break;

        // Check whether we have an interceptor at all.
        if (!this->runtime()->errorInterception.interceptor)
            break;

        // Make sure that we do not call the interceptor from within
        // the interceptor.
        this->runtime()->errorInterception.isExecuting = true;

        // The interceptor must be infallible.
        const mozilla::DebugOnly<bool> wasExceptionPending = this->isExceptionPending();
        this->runtime()->errorInterception.interceptor->interceptError(this, v);
        MOZ_ASSERT(wasExceptionPending == this->isExceptionPending());

        this->runtime()->errorInterception.isExecuting = false;
    } while (false);
#endif // defined(NIGHTLY_BUILD)

    // overRecursed_ is set after the fact by ReportOverRecursed.
    this->overRecursed_ = false;
    this->throwing = true;
    this->unwrappedException() = v;
    // We don't use assertSameCompartment here to allow
    // js::SetPendingExceptionCrossContext to work.
    MOZ_ASSERT_IF(v.isObject(), v.toObject().compartment() == compartment());
}

inline bool
JSContext::runningWithTrustedPrincipals()
{
    return !realm() || realm()->principals() == runtime()->trustedPrincipals();
}

inline void
JSContext::enterRealm(JS::Realm* realm)
{
    // We should never enter a realm while in the atoms zone.
    MOZ_ASSERT_IF(zone(), !zone()->isAtomsZone());

    realm->enter();
    setRealm(realm);
}

inline void
JSContext::enterAtomsZone(const js::AutoLockForExclusiveAccess& lock)
{
    // Only one thread can be in the atoms zone at a time.
    MOZ_ASSERT(runtime_->currentThreadHasExclusiveAccess());

    realm_ = nullptr;
    zone_ = runtime_->atomsZone(lock);
    arenas_ = &zone_->arenas;
}

inline void
JSContext::enterRealmOf(JSObject* target)
{
    MOZ_ASSERT(JS::CellIsNotGray(target));
    enterRealm(target->deprecatedRealm());
}

inline void
JSContext::enterRealmOf(JSScript* target)
{
    MOZ_ASSERT(JS::CellIsNotGray(target));
    enterRealm(target->realm());
}

inline void
JSContext::enterRealmOf(js::ObjectGroup* target)
{
    MOZ_ASSERT(JS::CellIsNotGray(target));
    enterRealm(target->realm());
}

inline void
JSContext::enterNullRealm()
{
    // We should never enter a realm while in the atoms zone.
    MOZ_ASSERT_IF(zone(), !zone()->isAtomsZone());

    setRealm(nullptr);
}

inline void
JSContext::leaveRealm(JS::Realm* oldRealm)
{
    // Only call leave() after we've setRealm()-ed away from the current realm.
    JS::Realm* startingRealm = realm_;

    // The current realm should be marked as entered-from-C++ at this point.
    MOZ_ASSERT_IF(startingRealm, startingRealm->hasBeenEnteredIgnoringJit());

    setRealm(oldRealm);

    if (startingRealm)
        startingRealm->leave();
}

inline void
JSContext::leaveAtomsZone(JS::Realm* oldRealm,
                          const js::AutoLockForExclusiveAccess& lock)
{
    setRealm(oldRealm);
}

inline void
JSContext::setRealm(JS::Realm* realm)
{
    realm_ = realm;
    if (realm) {
        // This thread must have exclusive access to the zone.
        MOZ_ASSERT(CurrentThreadCanAccessZone(realm->zone()));
        zone_ = realm->zone();
        arenas_ = &zone_->arenas;
    } else {
        zone_ = nullptr;
        arenas_ = nullptr;
    }
}

inline void
JSContext::setRealmForJitExceptionHandler(JS::Realm* realm)
{
    // JIT code enters (same-compartment) realms without calling realm->enter()
    // so we don't call realm->leave() here.
    MOZ_ASSERT(realm->compartment() == compartment());
    realm_ = realm;
}

inline JSScript*
JSContext::currentScript(jsbytecode** ppc, AllowCrossRealm allowCrossRealm) const
{
    if (ppc)
        *ppc = nullptr;

    js::Activation* act = activation();
    if (!act)
        return nullptr;

    MOZ_ASSERT(act->cx() == this);

    // Cross-compartment implies cross-realm.
    if (allowCrossRealm == AllowCrossRealm::DontAllow && act->compartment() != compartment())
        return nullptr;

    JSScript* script = nullptr;
    jsbytecode* pc = nullptr;
    if (act->isJit()) {
        if (act->hasWasmExitFP())
            return nullptr;
        js::jit::GetPcScript(const_cast<JSContext*>(this), &script, &pc);
    } else {
        js::InterpreterFrame* fp = act->asInterpreter()->current();
        MOZ_ASSERT(!fp->runningInJit());
        script = fp->script();
        pc = act->asInterpreter()->regs().pc;
    }

    MOZ_ASSERT(script->containsPC(pc));

    if (allowCrossRealm == AllowCrossRealm::DontAllow && script->realm() != realm())
        return nullptr;

    if (ppc)
        *ppc = pc;
    return script;
}

inline js::RuntimeCaches&
JSContext::caches()
{
    return runtime()->caches();
}

inline
js::AutoKeepAtoms::AutoKeepAtoms(JSContext* cx
                                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx(cx)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    cx->zone()->keepAtoms();
}

inline
js::AutoKeepAtoms::~AutoKeepAtoms()
{
    cx->zone()->releaseAtoms();
};

#endif /* vm_JSContext_inl_h */
