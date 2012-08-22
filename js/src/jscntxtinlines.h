/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscntxtinlines_h___
#define jscntxtinlines_h___

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsinterp.h"
#include "jsprobes.h"
#include "jsxml.h"
#include "jsgc.h"

#include "frontend/ParseMaps.h"
#include "vm/RegExpObject.h"

#include "jsgcinlines.h"

namespace js {

inline void
NewObjectCache::staticAsserts()
{
    JS_STATIC_ASSERT(NewObjectCache::MAX_OBJ_SIZE == sizeof(JSObject_Slots16));
    JS_STATIC_ASSERT(gc::FINALIZE_OBJECT_LAST == gc::FINALIZE_OBJECT16_BACKGROUND);
}

inline bool
NewObjectCache::lookup(Class *clasp, gc::Cell *key, gc::AllocKind kind, EntryIndex *pentry)
{
    uintptr_t hash = (uintptr_t(clasp) ^ uintptr_t(key)) + kind;
    *pentry = hash % js::ArrayLength(entries);

    Entry *entry = &entries[*pentry];

    /* N.B. Lookups with the same clasp/key but different kinds map to different entries. */
    return (entry->clasp == clasp && entry->key == key);
}

inline bool
NewObjectCache::lookupProto(Class *clasp, JSObject *proto, gc::AllocKind kind, EntryIndex *pentry)
{
    JS_ASSERT(!proto->isGlobal());
    return lookup(clasp, proto, kind, pentry);
}

inline bool
NewObjectCache::lookupGlobal(Class *clasp, js::GlobalObject *global, gc::AllocKind kind, EntryIndex *pentry)
{
    return lookup(clasp, global, kind, pentry);
}

inline bool
NewObjectCache::lookupType(Class *clasp, js::types::TypeObject *type, gc::AllocKind kind, EntryIndex *pentry)
{
    return lookup(clasp, type, kind, pentry);
}

inline void
NewObjectCache::fill(EntryIndex entry_, Class *clasp, gc::Cell *key, gc::AllocKind kind, JSObject *obj)
{
    JS_ASSERT(unsigned(entry_) < ArrayLength(entries));
    Entry *entry = &entries[entry_];

    JS_ASSERT(!obj->hasDynamicSlots() && !obj->hasDynamicElements());

    entry->clasp = clasp;
    entry->key = key;
    entry->kind = kind;

    entry->nbytes = obj->sizeOfThis();
    js_memcpy(&entry->templateObject, obj, entry->nbytes);
}

inline void
NewObjectCache::fillProto(EntryIndex entry, Class *clasp, JSObject *proto, gc::AllocKind kind, JSObject *obj)
{
    JS_ASSERT(!proto->isGlobal());
    JS_ASSERT(obj->getProto() == proto);
    return fill(entry, clasp, proto, kind, obj);
}

inline void
NewObjectCache::fillGlobal(EntryIndex entry, Class *clasp, js::GlobalObject *global, gc::AllocKind kind, JSObject *obj)
{
    //JS_ASSERT(global == obj->getGlobal());
    return fill(entry, clasp, global, kind, obj);
}

inline void
NewObjectCache::fillType(EntryIndex entry, Class *clasp, js::types::TypeObject *type, gc::AllocKind kind, JSObject *obj)
{
    JS_ASSERT(obj->type() == type);
    return fill(entry, clasp, type, kind, obj);
}

inline JSObject *
NewObjectCache::newObjectFromHit(JSContext *cx, EntryIndex entry_)
{
    JS_ASSERT(unsigned(entry_) < ArrayLength(entries));
    Entry *entry = &entries[entry_];

    JSObject *obj = js_TryNewGCObject(cx, entry->kind);
    if (obj) {
        copyCachedToObject(obj, reinterpret_cast<JSObject *>(&entry->templateObject));
        Probes::createObject(cx, obj);
        return obj;
    }

    return NULL;
}

struct PreserveRegsGuard
{
    PreserveRegsGuard(JSContext *cx, FrameRegs &regs)
      : prevContextRegs(cx->maybeRegs()), cx(cx), regs_(regs) {
        cx->stack.repointRegs(&regs_);
    }
    ~PreserveRegsGuard() {
        JS_ASSERT(cx->maybeRegs() == &regs_);
        *prevContextRegs = regs_;
        cx->stack.repointRegs(prevContextRegs);
    }

    FrameRegs *prevContextRegs;

  private:
    JSContext *cx;
    FrameRegs &regs_;
};

static inline GlobalObject *
GetGlobalForScopeChain(JSContext *cx)
{
    if (cx->hasfp())
        return &cx->fp()->global();

    JSObject *scope = JS_ObjectToInnerObject(cx, HandleObject::fromMarkedLocation(&cx->globalObject));
    if (!scope)
        return NULL;
    return &scope->asGlobal();
}

inline GSNCache *
GetGSNCache(JSContext *cx)
{
    return &cx->runtime->gsnCache;
}

#if JS_HAS_XML_SUPPORT

class AutoNamespaceArray : protected AutoGCRooter {
  public:
    AutoNamespaceArray(JSContext *cx)
        : AutoGCRooter(cx, NAMESPACES), context(cx) {
        array.init();
    }

    ~AutoNamespaceArray() {
        array.finish(context->runtime->defaultFreeOp());
    }

    uint32_t length() const { return array.length; }

  private:
    JSContext *context;
    friend void AutoGCRooter::trace(JSTracer *trc);

  public:
    JSXMLArray<JSObject> array;
};

#endif /* JS_HAS_XML_SUPPORT */

template <typename T>
class AutoPtr
{
    JSContext *cx;
    T *value;

    AutoPtr(const AutoPtr &other) MOZ_DELETE;

  public:
    explicit AutoPtr(JSContext *cx) : cx(cx), value(NULL) {}
    ~AutoPtr() {
        cx->delete_<T>(value);
    }

    void operator=(T *ptr) { value = ptr; }

    typedef void ***** ConvertibleToBool;
    operator ConvertibleToBool() const { return (ConvertibleToBool) value; }

    const T *operator->() const { return value; }
    T *operator->() { return value; }

    T *get() { return value; }
};

#ifdef DEBUG
class CompartmentChecker
{
    JSContext *context;
    JSCompartment *compartment;

  public:
    explicit CompartmentChecker(JSContext *cx)
      : context(cx), compartment(cx->compartment)
    {
        if (cx->compartment) {
            GlobalObject *global = GetGlobalForScopeChain(cx);
            JS_ASSERT(cx->global() == global);
        }
    }

    /*
     * Set a breakpoint here (break js::CompartmentChecker::fail) to debug
     * compartment mismatches.
     */
    static void fail(JSCompartment *c1, JSCompartment *c2) {
        printf("*** Compartment mismatch %p vs. %p\n", (void *) c1, (void *) c2);
        JS_NOT_REACHED("compartment mismatched");
    }

    /* Note: should only be used when neither c1 nor c2 may be the default compartment. */
    static void check(JSCompartment *c1, JSCompartment *c2) {
        JS_ASSERT(c1 != c1->rt->atomsCompartment);
        JS_ASSERT(c2 != c2->rt->atomsCompartment);
        if (c1 != c2)
            fail(c1, c2);
    }

    void check(JSCompartment *c) {
        if (c && c != context->runtime->atomsCompartment) {
            if (!compartment)
                compartment = c;
            else if (c != compartment)
                fail(compartment, c);
        }
    }

    void check(JSPrincipals *) { /* nothing for now */ }

    void check(JSObject *obj) {
        if (obj)
            check(obj->compartment());
    }

    template<typename T>
    void check(Handle<T> handle) {
        check(handle.get());
    }

    void check(JSString *str) {
        if (!str->isAtom())
            check(str->compartment());
    }

    void check(const js::Value &v) {
        if (v.isObject())
            check(&v.toObject());
        else if (v.isString())
            check(v.toString());
    }

    void check(const ValueArray &arr) {
        for (size_t i = 0; i < arr.length; i++)
            check(arr.array[i]);
    }

    void check(const JSValueArray &arr) {
        for (size_t i = 0; i < arr.length; i++)
            check(arr.array[i]);
    }

    void check(const CallArgs &args) {
        for (Value *p = args.base(); p != args.end(); ++p)
            check(*p);
    }

    void check(jsid id) {
        if (JSID_IS_OBJECT(id))
            check(JSID_TO_OBJECT(id));
    }

    void check(JSIdArray *ida) {
        if (ida) {
            for (int i = 0; i < ida->length; i++) {
                if (JSID_IS_OBJECT(ida->vector[i]))
                    check(ida->vector[i]);
            }
        }
    }

    void check(JSScript *script) {
        if (script)
            check(script->compartment());
    }

    void check(StackFrame *fp) {
        if (fp)
            check(fp->scopeChain());
    }
};

#endif

/*
 * Don't perform these checks when called from a finalizer. The checking
 * depends on other objects not having been swept yet.
 */
#define START_ASSERT_SAME_COMPARTMENT()                                       \
    if (cx->runtime->isHeapBusy())                                            \
        return;                                                               \
    CompartmentChecker c(cx)

template <class T1> inline void
assertSameCompartment(JSContext *cx, const T1 &t1)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
#endif
}

template <class T1, class T2> inline void
assertSameCompartment(JSContext *cx, const T1 &t1, const T2 &t2)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
#endif
}

template <class T1, class T2, class T3> inline void
assertSameCompartment(JSContext *cx, const T1 &t1, const T2 &t2, const T3 &t3)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
#endif
}

template <class T1, class T2, class T3, class T4> inline void
assertSameCompartment(JSContext *cx, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
    c.check(t4);
#endif
}

template <class T1, class T2, class T3, class T4, class T5> inline void
assertSameCompartment(JSContext *cx, const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5)
{
#ifdef DEBUG
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
JS_ALWAYS_INLINE bool
CallJSNative(JSContext *cx, Native native, const CallArgs &args)
{
    JS_CHECK_RECURSION(cx, return false);

#ifdef DEBUG
    bool alreadyThrowing = cx->isExceptionPending();
#endif
    assertSameCompartment(cx, args);
    bool ok = native(cx, args.length(), args.base());
    if (ok) {
        assertSameCompartment(cx, args.rval());
        JS_ASSERT_IF(!alreadyThrowing, !cx->isExceptionPending());
    }
    return ok;
}

STATIC_PRECONDITION_ASSUME(ubound(args.argv_) >= argc)
JS_ALWAYS_INLINE bool
CallNativeImpl(JSContext *cx, NativeImpl impl, const CallArgs &args)
{
#ifdef DEBUG
    bool alreadyThrowing = cx->isExceptionPending();
#endif
    assertSameCompartment(cx, args);
    bool ok = impl(cx, args);
    if (ok) {
        assertSameCompartment(cx, args.rval());
        JS_ASSERT_IF(!alreadyThrowing, !cx->isExceptionPending());
    }
    return ok;
}

extern JSBool CallOrConstructBoundFunction(JSContext *, unsigned, js::Value *);

STATIC_PRECONDITION(ubound(args.argv_) >= argc)
JS_ALWAYS_INLINE bool
CallJSNativeConstructor(JSContext *cx, Native native, const CallArgs &args)
{
#ifdef DEBUG
    RootedObject callee(cx, &args.callee());
#endif

    JS_ASSERT(args.thisv().isMagic());
    if (!CallJSNative(cx, native, args))
        return false;

    /*
     * Native constructors must return non-primitive values on success.
     * Although it is legal, if a constructor returns the callee, there is a
     * 99.9999% chance it is a bug. If any valid code actually wants the
     * constructor to return the callee, the assertion can be removed or
     * (another) conjunct can be added to the antecedent.
     *
     * Proxies are exceptions to both rules: they can return primitives and
     * they allow content to return the callee.
     *
     * CallOrConstructBoundFunction is an exception as well because we
     * might have used bind on a proxy function.
     *
     * (new Object(Object)) returns the callee.
     */
    JS_ASSERT_IF(native != FunctionProxyClass.construct &&
                 native != js::CallOrConstructBoundFunction &&
                 (!callee->isFunction() || callee->toFunction()->native() != js_Object),
                 !args.rval().isPrimitive() && callee != &args.rval().toObject());

    return true;
}

JS_ALWAYS_INLINE bool
CallJSPropertyOp(JSContext *cx, PropertyOp op, HandleObject receiver, HandleId id, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    assertSameCompartment(cx, receiver, id, vp);
    JSBool ok = op(cx, receiver, id, vp);
    if (ok)
        assertSameCompartment(cx, vp);
    return ok;
}

JS_ALWAYS_INLINE bool
CallJSPropertyOpSetter(JSContext *cx, StrictPropertyOp op, HandleObject obj, HandleId id,
                       JSBool strict, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    assertSameCompartment(cx, obj, id, vp);
    return op(cx, obj, id, strict, vp);
}

inline bool
CallSetter(JSContext *cx, HandleObject obj, HandleId id, StrictPropertyOp op, unsigned attrs,
           unsigned shortid, JSBool strict, MutableHandleValue vp)
{
    if (attrs & JSPROP_SETTER)
        return InvokeGetterOrSetter(cx, obj, CastAsObjectJsval(op), 1, vp.address(), vp.address());

    if (attrs & JSPROP_GETTER)
        return js_ReportGetterOnlyAssignment(cx);

    if (!(attrs & JSPROP_SHORTID))
        return CallJSPropertyOpSetter(cx, op, obj, id, strict, vp);

    RootedId nid(cx, INT_TO_JSID(shortid));

    return CallJSPropertyOpSetter(cx, op, obj, nid, strict, vp);
}

}  /* namespace js */

inline JSVersion
JSContext::findVersion() const
{
    if (hasVersionOverride)
        return versionOverride;

    if (stack.hasfp()) {
        /* There may be a scripted function somewhere on the stack! */
        js::StackFrame *f = fp();
        while (f && !f->isScriptFrame())
            f = f->prev();
        if (f)
            return f->script()->getVersion();
    }

    return defaultVersion;
}

inline bool
JSContext::canSetDefaultVersion() const
{
    return !stack.hasfp() && !hasVersionOverride;
}

inline void
JSContext::overrideVersion(JSVersion newVersion)
{
    JS_ASSERT(!canSetDefaultVersion());
    versionOverride = newVersion;
    hasVersionOverride = true;
}

inline bool
JSContext::maybeOverrideVersion(JSVersion newVersion)
{
    if (canSetDefaultVersion()) {
        setDefaultVersion(newVersion);
        return false;
    }
    overrideVersion(newVersion);
    return true;
}

inline unsigned
JSContext::getCompileOptions() const { return js::VersionFlagsToOptions(findVersion()); }

inline unsigned
JSContext::allOptions() const { return getRunOptions() | getCompileOptions(); }

inline void
JSContext::setCompileOptions(unsigned newcopts)
{
    JS_ASSERT((newcopts & JSCOMPILEOPTION_MASK) == newcopts);
    if (JS_LIKELY(getCompileOptions() == newcopts))
        return;
    JSVersion version = findVersion();
    JSVersion newVersion = js::OptionFlagsToVersion(newcopts, version);
    maybeOverrideVersion(newVersion);
}


inline js::LifoAlloc &
JSContext::typeLifoAlloc()
{
    return compartment->typeLifoAlloc;
}

inline void
JSContext::setPendingException(js::Value v) {
    JS_ASSERT(!IsPoisonedValue(v));
    this->throwing = true;
    this->exception = v;
    js::assertSameCompartment(this, v);
}

inline bool
JSContext::ensureParseMapPool()
{
    if (parseMapPool_)
        return true;
    parseMapPool_ = js::OffTheBooks::new_<js::frontend::ParseMapPool>(this);
    return parseMapPool_;
}

inline js::PropertyTree&
JSContext::propertyTree()
{
    return compartment->propertyTree;
}

/* Get the current frame, first lazily instantiating stack frames if needed. */
static inline js::StackFrame *
js_GetTopStackFrame(JSContext *cx, FrameExpandKind expand)
{
#ifdef JS_METHODJIT
    if (expand)
        js::mjit::ExpandInlineFrames(cx->compartment);
#endif

    return cx->maybefp();
}

#endif /* jscntxtinlines_h___ */
