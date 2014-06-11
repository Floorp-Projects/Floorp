/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartmentinlines.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsproxy.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#include "gc/Marking.h"
#ifdef JS_ION
#include "jit/JitCompartment.h"
#endif
#include "js/RootingAPI.h"
#include "vm/StopIterationObject.h"
#include "vm/WrapperObject.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;

JSCompartment::JSCompartment(Zone *zone, const JS::CompartmentOptions &options = JS::CompartmentOptions())
  : options_(options),
    zone_(zone),
    runtime_(zone->runtimeFromMainThread()),
    principals(nullptr),
    isSystem(false),
    isSelfHosting(false),
    marked(true),
#ifdef DEBUG
    firedOnNewGlobalObject(false),
#endif
    global_(nullptr),
    enterCompartmentDepth(0),
    data(nullptr),
    objectMetadataCallback(nullptr),
    lastAnimationTime(0),
    regExps(runtime_),
    globalWriteBarriered(false),
    propertyTree(thisForCtor()),
    selfHostingScriptSource(nullptr),
    gcIncomingGrayPointers(nullptr),
    gcWeakMapList(nullptr),
    debugModeBits(runtime_->debugMode ? DebugFromC : 0),
    rngState(0),
    watchpointMap(nullptr),
    scriptCountsMap(nullptr),
    debugScriptMap(nullptr),
    debugScopes(nullptr),
    enumerators(nullptr),
    compartmentStats(nullptr)
#ifdef JS_ION
    , jitCompartment_(nullptr)
#endif
{
    runtime_->numCompartments++;
    JS_ASSERT_IF(options.mergeable(), options.invisibleToDebugger());
}

JSCompartment::~JSCompartment()
{
#ifdef JS_ION
    js_delete(jitCompartment_);
#endif

    js_delete(watchpointMap);
    js_delete(scriptCountsMap);
    js_delete(debugScriptMap);
    js_delete(debugScopes);
    js_free(enumerators);

    runtime_->numCompartments--;
}

bool
JSCompartment::init(JSContext *cx)
{
    /*
     * As a hack, we clear our timezone cache every time we create a new
     * compartment. This ensures that the cache is always relatively fresh, but
     * shouldn't interfere with benchmarks which create tons of date objects
     * (unless they also create tons of iframes, which seems unlikely).
     */
    if (cx)
        cx->runtime()->dateTimeInfo.updateTimeZoneAdjustment();

    activeAnalysis = false;

    if (!crossCompartmentWrappers.init(0))
        return false;

    if (!regExps.init(cx))
        return false;

    enumerators = NativeIterator::allocateSentinel(cx);
    if (!enumerators)
        return false;

    if (!savedStacks_.init())
        return false;

    return debuggees.init(0);
}

#ifdef JS_ION
jit::JitRuntime *
JSRuntime::createJitRuntime(JSContext *cx)
{
    // The shared stubs are created in the atoms compartment, which may be
    // accessed by other threads with an exclusive context.
    AutoLockForExclusiveAccess atomsLock(cx);

    // The runtime will only be created on its owning thread, but reads of a
    // runtime's jitRuntime() can occur when another thread is requesting an
    // interrupt.
    AutoLockForInterrupt lock(this);

    JS_ASSERT(!jitRuntime_);

    jitRuntime_ = cx->new_<jit::JitRuntime>();

    if (!jitRuntime_)
        return nullptr;

    if (!jitRuntime_->initialize(cx)) {
        js_delete(jitRuntime_);
        jitRuntime_ = nullptr;

        JSCompartment *comp = cx->runtime()->atomsCompartment();
        if (comp->jitCompartment_) {
            js_delete(comp->jitCompartment_);
            comp->jitCompartment_ = nullptr;
        }

        return nullptr;
    }

    return jitRuntime_;
}

bool
JSCompartment::ensureJitCompartmentExists(JSContext *cx)
{
    using namespace js::jit;
    if (jitCompartment_)
        return true;

    if (!zone()->getJitZone(cx))
        return false;

    /* Set the compartment early, so linking works. */
    jitCompartment_ = cx->new_<JitCompartment>();

    if (!jitCompartment_)
        return false;

    if (!jitCompartment_->initialize(cx)) {
        js_delete(jitCompartment_);
        jitCompartment_ = nullptr;
        return false;
    }

    return true;
}
#endif

#ifdef JSGC_GENERATIONAL

/*
 * This class is used to add a post barrier on the crossCompartmentWrappers map,
 * as the key is calculated based on objects which may be moved by generational
 * GC.
 */
class WrapperMapRef : public BufferableRef
{
    WrapperMap *map;
    CrossCompartmentKey key;

  public:
    WrapperMapRef(WrapperMap *map, const CrossCompartmentKey &key)
      : map(map), key(key) {}

    void mark(JSTracer *trc) {
        CrossCompartmentKey prior = key;
        if (key.debugger)
            Mark(trc, &key.debugger, "CCW debugger");
        if (key.kind != CrossCompartmentKey::StringWrapper)
            Mark(trc, reinterpret_cast<JSObject**>(&key.wrapped), "CCW wrapped object");
        if (key.debugger == prior.debugger && key.wrapped == prior.wrapped)
            return;

        /* Look for the original entry, which might have been removed. */
        WrapperMap::Ptr p = map->lookup(prior);
        if (!p)
            return;

        /* Rekey the entry. */
        map->rekeyAs(prior, key, key);
    }
};

#ifdef JS_GC_ZEAL
void
JSCompartment::checkWrapperMapAfterMovingGC()
{
    /*
     * Assert that the postbarriers have worked and that nothing is left in
     * wrapperMap that points into the nursery, and that the hash table entries
     * are discoverable.
     */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey key = e.front().key();
        JS_ASSERT(!IsInsideNursery(key.debugger));
        JS_ASSERT(!IsInsideNursery(key.wrapped));
        JS_ASSERT(!IsInsideNursery(static_cast<Cell *>(e.front().value().get().toGCThing())));

        WrapperMap::Ptr ptr = crossCompartmentWrappers.lookup(key);
        JS_ASSERT(ptr.found() && &*ptr == &e.front());
    }
}
#endif

#endif

bool
JSCompartment::putWrapper(JSContext *cx, const CrossCompartmentKey &wrapped, const js::Value &wrapper)
{
    JS_ASSERT(wrapped.wrapped);
    JS_ASSERT(!IsPoisonedPtr(wrapped.wrapped));
    JS_ASSERT(!IsPoisonedPtr(wrapped.debugger));
    JS_ASSERT(!IsPoisonedPtr(wrapper.toGCThing()));
    JS_ASSERT_IF(wrapped.kind == CrossCompartmentKey::StringWrapper, wrapper.isString());
    JS_ASSERT_IF(wrapped.kind != CrossCompartmentKey::StringWrapper, wrapper.isObject());
    bool success = crossCompartmentWrappers.put(wrapped, ReadBarriered<Value>(wrapper));

#ifdef JSGC_GENERATIONAL
    /* There's no point allocating wrappers in the nursery since we will tenure them anyway. */
    JS_ASSERT(!IsInsideNursery(static_cast<gc::Cell *>(wrapper.toGCThing())));

    if (success && (IsInsideNursery(wrapped.wrapped) || IsInsideNursery(wrapped.debugger))) {
        WrapperMapRef ref(&crossCompartmentWrappers, wrapped);
        cx->runtime()->gc.storeBuffer.putGeneric(ref);
    }
#endif

    return success;
}

bool
JSCompartment::wrap(JSContext *cx, JSString **strp)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(this));
    JS_ASSERT(cx->compartment() == this);

    /* If the string is already in this compartment, we are done. */
    JSString *str = *strp;
    if (str->zoneFromAnyThread() == zone())
        return true;

    /* If the string is an atom, we don't have to copy. */
    if (str->isAtom()) {
        JS_ASSERT(str->isPermanentAtom() ||
                  cx->runtime()->isAtomsZone(str->zone()));
        return true;
    }

    /* Check the cache. */
    RootedValue key(cx, StringValue(str));
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
        *strp = p->value().get().toString();
        return true;
    }

    /*
     * No dice. Make a copy, and cache it. Directly allocate the copy in the
     * destination compartment, rather than first flattening it (and possibly
     * allocating in source compartment), because we don't know whether the
     * flattening will pay off later.
     */
    JSString *copy;
    if (str->hasPureChars()) {
        copy = js_NewStringCopyN<CanGC>(cx, str->pureChars(), str->length());
    } else {
        ScopedJSFreePtr<jschar> copiedChars;
        if (!str->copyNonPureCharsZ(cx, copiedChars))
            return false;
        copy = js_NewString<CanGC>(cx, copiedChars.forget(), str->length());
    }

    if (!copy)
        return false;
    if (!putWrapper(cx, CrossCompartmentKey(key), StringValue(copy)))
        return false;

    *strp = copy;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, HeapPtrString *strp)
{
    RootedString str(cx, *strp);
    if (!wrap(cx, str.address()))
        return false;
    *strp = str;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, MutableHandleObject obj, HandleObject existingArg)
{
    JS_ASSERT(!cx->runtime()->isAtomsCompartment(this));
    JS_ASSERT(cx->compartment() == this);
    JS_ASSERT_IF(existingArg, existingArg->compartment() == cx->compartment());
    JS_ASSERT_IF(existingArg, IsDeadProxyObject(existingArg));

    if (!obj)
        return true;
    AutoDisableProxyCheck adpc(cx->runtime());

    // Wrappers should really be parented to the wrapped parent of the wrapped
    // object, but in that case a wrapped global object would have a nullptr
    // parent without being a proper global object (JSCLASS_IS_GLOBAL). Instead,
    // we parent all wrappers to the global object in their home compartment.
    // This loses us some transparency, and is generally very cheesy.
    HandleObject global = cx->global();
    RootedObject objGlobal(cx, &obj->global());
    JS_ASSERT(global);
    JS_ASSERT(objGlobal);

    const JSWrapObjectCallbacks *cb = cx->runtime()->wrapObjectCallbacks;

    if (obj->compartment() == this) {
        obj.set(GetOuterObject(cx, obj));
        return true;
    }

    // If we have a cross-compartment wrapper, make sure that the cx isn't
    // associated with the self-hosting global. We don't want to create
    // wrappers for objects in other runtimes, which may be the case for the
    // self-hosting global.
    JS_ASSERT(!cx->runtime()->isSelfHostingGlobal(global) &&
              !cx->runtime()->isSelfHostingGlobal(objGlobal));

    // Unwrap the object, but don't unwrap outer windows.
    unsigned flags = 0;
    obj.set(UncheckedUnwrap(obj, /* stopAtOuter = */ true, &flags));

    if (obj->compartment() == this) {
        MOZ_ASSERT(obj == GetOuterObject(cx, obj));
        return true;
    }

    // Translate StopIteration singleton.
    if (obj->is<StopIterationObject>()) {
        // StopIteration isn't a constructor, but it's stored in GlobalObject
        // as one, out of laziness. Hence the GetBuiltinConstructor call here.
        RootedObject stopIteration(cx);
        if (!GetBuiltinConstructor(cx, JSProto_StopIteration, &stopIteration))
            return false;
        obj.set(stopIteration);
        return true;
    }

    // Invoke the prewrap callback. We're a bit worried about infinite
    // recursion here, so we do a check - see bug 809295.
    JS_CHECK_CHROME_RECURSION(cx, return false);
    if (cb->preWrap) {
        obj.set(cb->preWrap(cx, global, obj, flags));
        if (!obj)
            return false;
    }
    MOZ_ASSERT(obj == GetOuterObject(cx, obj));

    if (obj->compartment() == this)
        return true;


    // If we already have a wrapper for this value, use it.
    RootedValue key(cx, ObjectValue(*obj));
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
        obj.set(&p->value().get().toObject());
        JS_ASSERT(obj->is<CrossCompartmentWrapperObject>());
        JS_ASSERT(obj->getParent() == global);
        return true;
    }

    RootedObject proto(cx, TaggedProto::LazyProto);
    RootedObject existing(cx, existingArg);
    if (existing) {
        // Is it possible to reuse |existing|?
        if (!existing->getTaggedProto().isLazy() ||
            // Note: don't use is<ObjectProxyObject>() here -- it also matches subclasses!
            existing->getClass() != &ProxyObject::uncallableClass_ ||
            existing->getParent() != global ||
            obj->isCallable())
        {
            existing = nullptr;
        }
    }

    obj.set(cb->wrap(cx, existing, obj, proto, global, flags));
    if (!obj)
        return false;

    // We maintain the invariant that the key in the cross-compartment wrapper
    // map is always directly wrapped by the value.
    JS_ASSERT(Wrapper::wrappedObject(obj) == &key.get().toObject());

    return putWrapper(cx, CrossCompartmentKey(key), ObjectValue(*obj));
}

bool
JSCompartment::wrapId(JSContext *cx, jsid *idp)
{
    MOZ_ASSERT(*idp != JSID_VOID, "JSID_VOID is an out-of-band sentinel value");
    if (JSID_IS_INT(*idp))
        return true;
    RootedValue value(cx, IdToValue(*idp));
    if (!wrap(cx, &value))
        return false;
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, value, &id))
        return false;

    *idp = id;
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyOp *propp)
{
    RootedValue value(cx, CastAsObjectJsval(*propp));
    if (!wrap(cx, &value))
        return false;
    *propp = CastAsPropertyOp(value.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, StrictPropertyOp *propp)
{
    RootedValue value(cx, CastAsObjectJsval(*propp));
    if (!wrap(cx, &value))
        return false;
    *propp = CastAsStrictPropertyOp(value.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, MutableHandle<PropertyDescriptor> desc)
{
    if (!wrap(cx, desc.object()))
        return false;

    if (desc.hasGetterObject()) {
        if (!wrap(cx, &desc.getter()))
            return false;
    }
    if (desc.hasSetterObject()) {
        if (!wrap(cx, &desc.setter()))
            return false;
    }

    return wrap(cx, desc.value());
}

bool
JSCompartment::wrap(JSContext *cx, AutoIdVector &props)
{
    jsid *vector = props.begin();
    int length = props.length();
    for (size_t n = 0; n < size_t(length); ++n) {
        if (!wrapId(cx, &vector[n]))
            return false;
    }
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, MutableHandle<PropDesc> desc)
{
    if (desc.isUndefined())
        return true;

    JSCompartment *comp = cx->compartment();

    if (desc.hasValue()) {
        RootedValue value(cx, desc.value());
        if (!comp->wrap(cx, &value))
            return false;
        desc.setValue(value);
    }
    if (desc.hasGet()) {
        RootedValue get(cx, desc.getterValue());
        if (!comp->wrap(cx, &get))
            return false;
        desc.setGetter(get);
    }
    if (desc.hasSet()) {
        RootedValue set(cx, desc.setterValue());
        if (!comp->wrap(cx, &set))
            return false;
        desc.setSetter(set);
    }
    if (desc.descriptorValue().isObject()) {
        RootedObject descObj(cx, &desc.descriptorValue().toObject());
        if (!comp->wrap(cx, &descObj))
            return false;
        desc.setDescriptorObject(descObj);
    }
    return true;
}

/*
 * This method marks pointers that cross compartment boundaries. It should be
 * called only for per-compartment GCs, since full GCs naturally follow pointers
 * across compartments.
 */
void
JSCompartment::markCrossCompartmentWrappers(JSTracer *trc)
{
    JS_ASSERT(!zone()->isCollecting());

    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        Value v = e.front().value();
        if (e.front().key().kind == CrossCompartmentKey::ObjectWrapper) {
            ProxyObject *wrapper = &v.toObject().as<ProxyObject>();

            /*
             * We have a cross-compartment wrapper. Its private pointer may
             * point into the compartment being collected, so we should mark it.
             */
            Value referent = wrapper->private_();
            MarkValueRoot(trc, &referent, "cross-compartment wrapper");
            JS_ASSERT(referent == wrapper->private_());
        }
    }
}

void
JSCompartment::trace(JSTracer *trc)
{
    // At the moment, this is merely ceremonial, but any live-compartment-only tracing should go
    // here.
}

void
JSCompartment::markRoots(JSTracer *trc)
{
    JS_ASSERT(!trc->runtime()->isHeapMinorCollecting());

#ifdef JS_ION
    if (jitCompartment_)
        jitCompartment_->mark(trc, this);
#endif

    /*
     * If a compartment is on-stack, we mark its global so that
     * JSContext::global() remains valid.
     */
    if (enterCompartmentDepth && global_)
        MarkObjectRoot(trc, global_.unsafeGet(), "on-stack compartment global");
}

void
JSCompartment::sweep(FreeOp *fop, bool releaseTypes)
{
    JS_ASSERT(!activeAnalysis);

    /* This function includes itself in PHASE_SWEEP_TABLES. */
    sweepCrossCompartmentWrappers();

    JSRuntime *rt = runtimeFromMainThread();

    {
        gcstats::AutoPhase ap(rt->gc.stats, gcstats::PHASE_SWEEP_TABLES);

        /* Remove dead references held weakly by the compartment. */

        sweepBaseShapeTable();
        sweepInitialShapeTable();
        sweepNewTypeObjectTable(newTypeObjects);
        sweepNewTypeObjectTable(lazyTypeObjects);
        sweepCallsiteClones();
        savedStacks_.sweep(rt);

        if (global_ && IsObjectAboutToBeFinalized(global_.unsafeGet()))
            global_.set(nullptr);

        if (selfHostingScriptSource &&
            IsObjectAboutToBeFinalized((JSObject **) selfHostingScriptSource.unsafeGet()))
        {
            selfHostingScriptSource.set(nullptr);
        }

#ifdef JS_ION
        if (jitCompartment_)
            jitCompartment_->sweep(fop);
#endif

        /*
         * JIT code increments activeUseCount for any RegExpShared used by jit
         * code for the lifetime of the JIT script. Thus, we must perform
         * sweeping after clearing jit code.
         */
        regExps.sweep(rt);

        if (debugScopes)
            debugScopes->sweep(rt);

        /* Finalize unreachable (key,value) pairs in all weak maps. */
        WeakMapBase::sweepCompartment(this);
    }

    NativeIterator *ni = enumerators->next();
    while (ni != enumerators) {
        JSObject *iterObj = ni->iterObj();
        NativeIterator *next = ni->next();
        if (gc::IsObjectAboutToBeFinalized(&iterObj))
            ni->unlink();
        ni = next;
    }
}

/*
 * Remove dead wrappers from the table. We must sweep all compartments, since
 * string entries in the crossCompartmentWrappers table are not marked during
 * markCrossCompartmentWrappers.
 */
void
JSCompartment::sweepCrossCompartmentWrappers()
{
    JSRuntime *rt = runtimeFromMainThread();

    gcstats::AutoPhase ap1(rt->gc.stats, gcstats::PHASE_SWEEP_TABLES);
    gcstats::AutoPhase ap2(rt->gc.stats, gcstats::PHASE_SWEEP_TABLES_WRAPPER);

    /* Remove dead wrappers from the table. */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey key = e.front().key();
        bool keyDying = IsCellAboutToBeFinalized(&key.wrapped);
        bool valDying = IsValueAboutToBeFinalized(e.front().value().unsafeGet());
        bool dbgDying = key.debugger && IsObjectAboutToBeFinalized(&key.debugger);
        if (keyDying || valDying || dbgDying) {
            JS_ASSERT(key.kind != CrossCompartmentKey::StringWrapper);
            e.removeFront();
        } else if (key.wrapped != e.front().key().wrapped ||
                   key.debugger != e.front().key().debugger)
        {
            e.rekeyFront(key);
        }
    }
}

void
JSCompartment::purge()
{
    dtoaCache.purge();
}

void
JSCompartment::clearTables()
{
    global_.set(nullptr);

    // No scripts should have run in this compartment. This is used when
    // merging a compartment that has been used off thread into another
    // compartment and zone.
    JS_ASSERT(crossCompartmentWrappers.empty());
    JS_ASSERT_IF(callsiteClones.initialized(), callsiteClones.empty());
#ifdef JS_ION
    JS_ASSERT(!jitCompartment_);
#endif
    JS_ASSERT(!debugScopes);
    JS_ASSERT(!gcWeakMapList);
    JS_ASSERT(enumerators->next() == enumerators);
    JS_ASSERT(regExps.empty());

    types.clearTables();
    if (baseShapes.initialized())
        baseShapes.clear();
    if (initialShapes.initialized())
        initialShapes.clear();
    if (newTypeObjects.initialized())
        newTypeObjects.clear();
    if (lazyTypeObjects.initialized())
        lazyTypeObjects.clear();
    if (savedStacks_.initialized())
        savedStacks_.clear();
}

void
JSCompartment::setObjectMetadataCallback(js::ObjectMetadataCallback callback)
{
    // Clear any jitcode in the runtime, which behaves differently depending on
    // whether there is a creation callback.
    ReleaseAllJITCode(runtime_->defaultFreeOp());

    objectMetadataCallback = callback;
}

bool
JSCompartment::hasScriptsOnStack()
{
    for (ActivationIterator iter(runtimeFromMainThread()); !iter.done(); ++iter) {
        if (iter->compartment() == this)
            return true;
    }

    return false;
}

static bool
AddInnerLazyFunctionsFromScript(JSScript *script, AutoObjectVector &lazyFunctions)
{
    if (!script->hasObjects())
        return true;
    ObjectArray *objects = script->objects();
    for (size_t i = script->innerObjectsStart(); i < objects->length; i++) {
        JSObject *obj = objects->vector[i];
        if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpretedLazy()) {
            if (!lazyFunctions.append(obj))
                return false;
        }
    }
    return true;
}

static bool
CreateLazyScriptsForCompartment(JSContext *cx)
{
    AutoObjectVector lazyFunctions(cx);

    // Find all live lazy scripts in the compartment, and via them all root
    // lazy functions in the compartment: those which have not been compiled,
    // which have a source object, indicating that they have a parent, and
    // which do not have an uncompiled enclosing script. The last condition is
    // so that we don't compile lazy scripts whose enclosing scripts failed to
    // compile, indicating that the lazy script did not escape the script.
    for (gc::ZoneCellIter i(cx->zone(), gc::FINALIZE_LAZY_SCRIPT); !i.done(); i.next()) {
        LazyScript *lazy = i.get<LazyScript>();
        JSFunction *fun = lazy->functionNonDelazifying();
        if (fun->compartment() == cx->compartment() &&
            lazy->sourceObject() && !lazy->maybeScript() &&
            !lazy->hasUncompiledEnclosingScript())
        {
            MOZ_ASSERT(fun->isInterpretedLazy());
            MOZ_ASSERT(lazy == fun->lazyScriptOrNull());
            if (!lazyFunctions.append(fun))
                return false;
        }
    }

    // Create scripts for each lazy function, updating the list of functions to
    // process with any newly exposed inner functions in created scripts.
    // A function cannot be delazified until its outer script exists.
    for (size_t i = 0; i < lazyFunctions.length(); i++) {
        JSFunction *fun = &lazyFunctions[i]->as<JSFunction>();

        // lazyFunctions may have been populated with multiple functions for
        // a lazy script.
        if (!fun->isInterpretedLazy())
            continue;

        JSScript *script = fun->getOrCreateScript(cx);
        if (!script)
            return false;
        if (!AddInnerLazyFunctionsFromScript(script, lazyFunctions))
            return false;
    }

    return true;
}

bool
JSCompartment::ensureDelazifyScriptsForDebugMode(JSContext *cx)
{
    MOZ_ASSERT(cx->compartment() == this);
    if ((debugModeBits & DebugNeedDelazification) && !CreateLazyScriptsForCompartment(cx))
        return false;
    debugModeBits &= ~DebugNeedDelazification;
    return true;
}

bool
JSCompartment::setDebugModeFromC(JSContext *cx, bool b, AutoDebugModeInvalidation &invalidate)
{
    bool enabledBefore = debugMode();
    bool enabledAfter = (debugModeBits & DebugModeFromMask & ~DebugFromC) || b;

    // Enabling debug mode from C (vs of from JS) can only be done when no
    // scripts from the target compartment are on the stack.
    //
    // We do allow disabling debug mode while scripts are on the stack.  In
    // that case the debug-mode code for those scripts remains, so subsequently
    // hooks may be called erroneously, even though debug mode is supposedly
    // off, and we have to live with it.
    bool onStack = false;
    if (enabledBefore != enabledAfter) {
        onStack = hasScriptsOnStack();
        if (b && onStack) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_DEBUG_NOT_IDLE);
            return false;
        }
    }

    debugModeBits = (debugModeBits & ~DebugFromC) | (b ? DebugFromC : 0);
    JS_ASSERT(debugMode() == enabledAfter);
    if (enabledBefore != enabledAfter) {
        // Pass in a nullptr cx to not bother recompiling for JSD1, since
        // we're still enforcing the idle-stack invariant here.
        if (!updateJITForDebugMode(nullptr, invalidate))
            return false;
        if (!enabledAfter)
            DebugScopes::onCompartmentLeaveDebugMode(this);
    }
    return true;
}

bool
JSCompartment::updateJITForDebugMode(JSContext *maybecx, AutoDebugModeInvalidation &invalidate)
{
#ifdef JS_ION
    // The AutoDebugModeInvalidation argument makes sure we can't forget to
    // invalidate, but it is also important not to run any scripts in this
    // compartment until the invalidate is destroyed.  That is the caller's
    // responsibility.
    if (!jit::UpdateForDebugMode(maybecx, this, invalidate))
        return false;
#endif
    return true;
}

bool
JSCompartment::addDebuggee(JSContext *cx, js::GlobalObject *global)
{
    AutoDebugModeInvalidation invalidate(this);
    return addDebuggee(cx, global, invalidate);
}

bool
JSCompartment::addDebuggee(JSContext *cx,
                           GlobalObject *globalArg,
                           AutoDebugModeInvalidation &invalidate)
{
    Rooted<GlobalObject*> global(cx, globalArg);

    bool wasEnabled = debugMode();
    if (!debuggees.put(global)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    debugModeBits |= DebugFromJS;
    if (!wasEnabled && !updateJITForDebugMode(cx, invalidate))
        return false;
    return true;
}

bool
JSCompartment::removeDebuggee(JSContext *cx,
                              js::GlobalObject *global,
                              js::GlobalObjectSet::Enum *debuggeesEnum)
{
    AutoDebugModeInvalidation invalidate(this);
    return removeDebuggee(cx, global, invalidate, debuggeesEnum);
}

bool
JSCompartment::removeDebuggee(JSContext *cx,
                              js::GlobalObject *global,
                              AutoDebugModeInvalidation &invalidate,
                              js::GlobalObjectSet::Enum *debuggeesEnum)
{
    bool wasEnabled = debugMode();
    removeDebuggeeUnderGC(cx->runtime()->defaultFreeOp(), global, invalidate, debuggeesEnum);
    if (wasEnabled && !debugMode() && !updateJITForDebugMode(cx, invalidate))
        return false;
    return true;
}

void
JSCompartment::removeDebuggeeUnderGC(FreeOp *fop,
                                     js::GlobalObject *global,
                                     js::GlobalObjectSet::Enum *debuggeesEnum)
{
    AutoDebugModeInvalidation invalidate(this);
    removeDebuggeeUnderGC(fop, global, invalidate, debuggeesEnum);
}

void
JSCompartment::removeDebuggeeUnderGC(FreeOp *fop,
                                     js::GlobalObject *global,
                                     AutoDebugModeInvalidation &invalidate,
                                     js::GlobalObjectSet::Enum *debuggeesEnum)
{
    bool wasEnabled = debugMode();
    JS_ASSERT(debuggees.has(global));
    if (debuggeesEnum)
        debuggeesEnum->removeFront();
    else
        debuggees.remove(global);

    if (debuggees.empty()) {
        debugModeBits &= ~DebugFromJS;
        if (wasEnabled && !debugMode())
            DebugScopes::onCompartmentLeaveDebugMode(this);
    }
}

void
JSCompartment::clearBreakpointsIn(FreeOp *fop, js::Debugger *dbg, HandleObject handler)
{
    for (gc::ZoneCellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() == this && script->hasAnyBreakpointsOrStepMode())
            script->clearBreakpointsIn(fop, dbg, handler);
    }
}

void
JSCompartment::clearTraps(FreeOp *fop)
{
    MinorGC(fop->runtime(), JS::gcreason::EVICT_NURSERY);
    for (gc::ZoneCellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() == this && script->hasAnyBreakpointsOrStepMode())
            script->clearTraps(fop);
    }
}

void
JSCompartment::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                      size_t *tiAllocationSiteTables,
                                      size_t *tiArrayTypeTables,
                                      size_t *tiObjectTypeTables,
                                      size_t *compartmentObject,
                                      size_t *shapesCompartmentTables,
                                      size_t *crossCompartmentWrappersArg,
                                      size_t *regexpCompartment,
                                      size_t *debuggeesSet,
                                      size_t *savedStacksSet)
{
    *compartmentObject += mallocSizeOf(this);
    types.addSizeOfExcludingThis(mallocSizeOf, tiAllocationSiteTables,
                                 tiArrayTypeTables, tiObjectTypeTables);
    *shapesCompartmentTables += baseShapes.sizeOfExcludingThis(mallocSizeOf)
                              + initialShapes.sizeOfExcludingThis(mallocSizeOf)
                              + newTypeObjects.sizeOfExcludingThis(mallocSizeOf)
                              + lazyTypeObjects.sizeOfExcludingThis(mallocSizeOf);
    *crossCompartmentWrappersArg += crossCompartmentWrappers.sizeOfExcludingThis(mallocSizeOf);
    *regexpCompartment += regExps.sizeOfExcludingThis(mallocSizeOf);
    *debuggeesSet += debuggees.sizeOfExcludingThis(mallocSizeOf);
    *savedStacksSet += savedStacks_.sizeOfExcludingThis(mallocSizeOf);
}

void
JSCompartment::adoptWorkerAllocator(Allocator *workerAllocator)
{
    zone()->allocator.arenas.adoptArenas(runtimeFromMainThread(), &workerAllocator->arenas);
}
