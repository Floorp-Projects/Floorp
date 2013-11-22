/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Debugger_h
#define vm_Debugger_h

#include "mozilla/LinkedList.h"

#include "jsclist.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsweakmap.h"

#include "gc/Barrier.h"
#include "js/HashTable.h"
#include "vm/GlobalObject.h"

namespace js {

class Breakpoint;

/*
 * A weakmap that supports the keys being in different compartments to the
 * values, although all values must be in the same compartment.
 *
 * The Key and Value classes must support the compartment() method.
 *
 * The purpose of this is to allow the garbage collector to easily find edges
 * from debugee object compartments to debugger compartments when calculating
 * the compartment groups.  Note that these edges are the inverse of the edges
 * stored in the cross compartment map.
 *
 * The current implementation results in all debuggee object compartments being
 * swept in the same group as the debugger.  This is a conservative approach,
 * and compartments may be unnecessarily grouped, however it results in a
 * simpler and faster implementation.
 */
template <class Key, class Value>
class DebuggerWeakMap : private WeakMap<Key, Value, DefaultHasher<Key> >
{
  private:
    typedef HashMap<JS::Zone *,
                    uintptr_t,
                    DefaultHasher<JS::Zone *>,
                    RuntimeAllocPolicy> CountMap;

    CountMap zoneCounts;

  public:
    typedef WeakMap<Key, Value, DefaultHasher<Key> > Base;
    explicit DebuggerWeakMap(JSContext *cx)
        : Base(cx), zoneCounts(cx->runtime()) { }

  public:
    /* Expose those parts of HashMap public interface that are used by Debugger methods. */

    typedef typename Base::Entry Entry;
    typedef typename Base::Ptr Ptr;
    typedef typename Base::AddPtr AddPtr;
    typedef typename Base::Range Range;
    typedef typename Base::Enum Enum;
    typedef typename Base::Lookup Lookup;

    /* Expose WeakMap public interface */

    using Base::clearWithoutCallingDestructors;
    using Base::lookupForAdd;
    using Base::all;
    using Base::trace;

    bool init(uint32_t len = 16) {
        return Base::init(len) && zoneCounts.init();
    }

    template<typename KeyInput, typename ValueInput>
    bool relookupOrAdd(AddPtr &p, const KeyInput &k, const ValueInput &v) {
        JS_ASSERT(v->compartment() == Base::compartment);
        if (!incZoneCount(k->zone()))
            return false;
        bool ok = Base::relookupOrAdd(p, k, v);
        if (!ok)
            decZoneCount(k->zone());
        return ok;
    }

    void remove(const Lookup &l) {
        Base::remove(l);
        decZoneCount(l->zone());
    }

  public:
    void markKeys(JSTracer *tracer) {
        for (Enum e(*static_cast<Base *>(this)); !e.empty(); e.popFront()) {
            Key key = e.front().key;
            gc::Mark(tracer, &key, "Debugger WeakMap key");
            if (key != e.front().key)
                e.rekeyFront(key);
            key.unsafeSet(nullptr);
        }
    }

    bool hasKeyInZone(JS::Zone *zone) {
        CountMap::Ptr p = zoneCounts.lookup(zone);
        JS_ASSERT_IF(p, p->value > 0);
        return p;
    }

  private:
    /* Override sweep method to also update our edge cache. */
    void sweep() {
        for (Enum e(*static_cast<Base *>(this)); !e.empty(); e.popFront()) {
            Key k(e.front().key);
            if (gc::IsAboutToBeFinalized(&k)) {
                e.removeFront();
                decZoneCount(k->zone());
            }
        }
        Base::assertEntriesNotAboutToBeFinalized();
    }

    bool incZoneCount(JS::Zone *zone) {
        CountMap::Ptr p = zoneCounts.lookupWithDefault(zone, 0);
        if (!p)
            return false;
        ++p->value;
        return true;
    }

    void decZoneCount(JS::Zone *zone) {
        CountMap::Ptr p = zoneCounts.lookup(zone);
        JS_ASSERT(p);
        JS_ASSERT(p->value > 0);
        --p->value;
        if (p->value == 0)
            zoneCounts.remove(zone);
    }
};

/*
 * Env is the type of what ES5 calls "lexical environments" (runtime
 * activations of lexical scopes). This is currently just JSObject, and is
 * implemented by Call, Block, With, and DeclEnv objects, among others--but
 * environments and objects are really two different concepts.
 */
typedef JSObject Env;

class Debugger : private mozilla::LinkedListElement<Debugger>
{
    friend class Breakpoint;
    friend class mozilla::LinkedListElement<Debugger>;
    friend bool (::JS_DefineDebuggerObject)(JSContext *cx, JSObject *obj);

  public:
    enum Hook {
        OnDebuggerStatement,
        OnExceptionUnwind,
        OnNewScript,
        OnEnterFrame,
        OnNewGlobalObject,
        HookCount
    };

    enum {
        JSSLOT_DEBUG_PROTO_START,
        JSSLOT_DEBUG_FRAME_PROTO = JSSLOT_DEBUG_PROTO_START,
        JSSLOT_DEBUG_ENV_PROTO,
        JSSLOT_DEBUG_OBJECT_PROTO,
        JSSLOT_DEBUG_SCRIPT_PROTO,
        JSSLOT_DEBUG_SOURCE_PROTO,
        JSSLOT_DEBUG_PROTO_STOP,
        JSSLOT_DEBUG_HOOK_START = JSSLOT_DEBUG_PROTO_STOP,
        JSSLOT_DEBUG_HOOK_STOP = JSSLOT_DEBUG_HOOK_START + HookCount,
        JSSLOT_DEBUG_COUNT = JSSLOT_DEBUG_HOOK_STOP
    };

  private:
    HeapPtrObject object;               /* The Debugger object. Strong reference. */
    GlobalObjectSet debuggees;          /* Debuggee globals. Cross-compartment weak references. */
    js::HeapPtrObject uncaughtExceptionHook; /* Strong reference. */
    bool enabled;
    JSCList breakpoints;                /* Circular list of all js::Breakpoints in this debugger */

    /*
     * If this Debugger is enabled, and has a onNewGlobalObject handler, then
     * this link is inserted into the circular list headed by
     * JSRuntime::onNewGlobalObjectWatchers. Otherwise, this is set to a
     * singleton cycle.
     */
    JSCList onNewGlobalObjectWatchersLink;

    /*
     * Map from stack frames that are currently on the stack to Debugger.Frame
     * instances.
     *
     * The keys are always live stack frames. We drop them from this map as
     * soon as they leave the stack (see slowPathOnLeaveFrame) and in
     * removeDebuggee.
     *
     * We don't trace the keys of this map (the frames are on the stack and
     * thus necessarily live), but we do trace the values. It's like a WeakMap
     * that way, but since stack frames are not gc-things, the implementation
     * has to be different.
     */
    typedef HashMap<AbstractFramePtr,
                    RelocatablePtrObject,
                    DefaultHasher<AbstractFramePtr>,
                    RuntimeAllocPolicy> FrameMap;
    FrameMap frames;

    /* An ephemeral map from JSScript* to Debugger.Script instances. */
    typedef DebuggerWeakMap<EncapsulatedPtrScript, RelocatablePtrObject> ScriptWeakMap;
    ScriptWeakMap scripts;

    /* The map from debuggee source script objects to their Debugger.Source instances. */
    typedef DebuggerWeakMap<EncapsulatedPtrObject, RelocatablePtrObject> SourceWeakMap;
    SourceWeakMap sources;

    /* The map from debuggee objects to their Debugger.Object instances. */
    typedef DebuggerWeakMap<EncapsulatedPtrObject, RelocatablePtrObject> ObjectWeakMap;
    ObjectWeakMap objects;

    /* The map from debuggee Envs to Debugger.Environment instances. */
    ObjectWeakMap environments;

    class FrameRange;
    class ScriptQuery;

    bool addDebuggeeGlobal(JSContext *cx, Handle<GlobalObject*> obj);
    bool addDebuggeeGlobal(JSContext *cx, Handle<GlobalObject*> obj,
                           AutoDebugModeInvalidation &invalidate);
    void removeDebuggeeGlobal(FreeOp *fop, GlobalObject *global,
                              GlobalObjectSet::Enum *compartmentEnum,
                              GlobalObjectSet::Enum *debugEnum);
    void removeDebuggeeGlobal(FreeOp *fop, GlobalObject *global,
                              AutoDebugModeInvalidation &invalidate,
                              GlobalObjectSet::Enum *compartmentEnum,
                              GlobalObjectSet::Enum *debugEnum);

    /*
     * Cope with an error or exception in a debugger hook.
     *
     * If callHook is true, then call the uncaughtExceptionHook, if any. If, in
     * addition, vp is given, then parse the value returned by
     * uncaughtExceptionHook as a resumption value.
     *
     * If there is no uncaughtExceptionHook, or if it fails, report and clear
     * the pending exception on ac.context and return JSTRAP_ERROR.
     *
     * This always calls ac.leave(); ac is a parameter because this method must
     * do some things in the debugger compartment and some things in the
     * debuggee compartment.
     */
    JSTrapStatus handleUncaughtException(mozilla::Maybe<AutoCompartment> &ac, bool callHook);
    JSTrapStatus handleUncaughtException(mozilla::Maybe<AutoCompartment> &ac, MutableHandleValue vp, bool callHook);

    JSTrapStatus handleUncaughtExceptionHelper(mozilla::Maybe<AutoCompartment> &ac,
                                               MutableHandleValue *vp, bool callHook);

    /*
     * Handle the result of a hook that is expected to return a resumption
     * value <https://wiki.mozilla.org/Debugger#Resumption_Values>. This is called
     * when we return from a debugging hook to debuggee code. The interpreter wants
     * a (JSTrapStatus, Value) pair telling it how to proceed.
     *
     * Precondition: ac is entered. We are in the debugger compartment.
     *
     * Postcondition: This called ac.leave(). See handleUncaughtException.
     *
     * If ok is false, the hook failed. If an exception is pending in
     * ac.context(), return handleUncaughtException(ac, vp, callhook).
     * Otherwise just return JSTRAP_ERROR.
     *
     * If ok is true, there must be no exception pending in ac.context(). rv may be:
     *     undefined - Return JSTRAP_CONTINUE to continue execution normally.
     *     {return: value} or {throw: value} - Call unwrapDebuggeeValue to
     *         unwrap value. Store the result in *vp and return JSTRAP_RETURN
     *         or JSTRAP_THROW. The interpreter will force the current frame to
     *         return or throw an exception.
     *     null - Return JSTRAP_ERROR to terminate the debuggee with an
     *         uncatchable error.
     *     anything else - Make a new TypeError the pending exception and
     *         return handleUncaughtException(ac, vp, callHook).
     */
    JSTrapStatus parseResumptionValue(mozilla::Maybe<AutoCompartment> &ac, bool ok, const Value &rv,
                                      MutableHandleValue vp, bool callHook = true);

    GlobalObject *unwrapDebuggeeArgument(JSContext *cx, const Value &v);

    static void traceObject(JSTracer *trc, JSObject *obj);
    void trace(JSTracer *trc);
    static void finalize(FreeOp *fop, JSObject *obj);
    void markKeysInCompartment(JSTracer *tracer);

    static const Class jsclass;

    static Debugger *fromThisValue(JSContext *cx, const CallArgs &ca, const char *fnname);
    static bool getEnabled(JSContext *cx, unsigned argc, Value *vp);
    static bool setEnabled(JSContext *cx, unsigned argc, Value *vp);
    static bool getHookImpl(JSContext *cx, unsigned argc, Value *vp, Hook which);
    static bool setHookImpl(JSContext *cx, unsigned argc, Value *vp, Hook which);
    static bool getOnDebuggerStatement(JSContext *cx, unsigned argc, Value *vp);
    static bool setOnDebuggerStatement(JSContext *cx, unsigned argc, Value *vp);
    static bool getOnExceptionUnwind(JSContext *cx, unsigned argc, Value *vp);
    static bool setOnExceptionUnwind(JSContext *cx, unsigned argc, Value *vp);
    static bool getOnNewScript(JSContext *cx, unsigned argc, Value *vp);
    static bool setOnNewScript(JSContext *cx, unsigned argc, Value *vp);
    static bool getOnEnterFrame(JSContext *cx, unsigned argc, Value *vp);
    static bool setOnEnterFrame(JSContext *cx, unsigned argc, Value *vp);
    static bool getOnNewGlobalObject(JSContext *cx, unsigned argc, Value *vp);
    static bool setOnNewGlobalObject(JSContext *cx, unsigned argc, Value *vp);
    static bool getUncaughtExceptionHook(JSContext *cx, unsigned argc, Value *vp);
    static bool setUncaughtExceptionHook(JSContext *cx, unsigned argc, Value *vp);
    static bool addDebuggee(JSContext *cx, unsigned argc, Value *vp);
    static bool addAllGlobalsAsDebuggees(JSContext *cx, unsigned argc, Value *vp);
    static bool removeDebuggee(JSContext *cx, unsigned argc, Value *vp);
    static bool removeAllDebuggees(JSContext *cx, unsigned argc, Value *vp);
    static bool hasDebuggee(JSContext *cx, unsigned argc, Value *vp);
    static bool getDebuggees(JSContext *cx, unsigned argc, Value *vp);
    static bool getNewestFrame(JSContext *cx, unsigned argc, Value *vp);
    static bool clearAllBreakpoints(JSContext *cx, unsigned argc, Value *vp);
    static bool findScripts(JSContext *cx, unsigned argc, Value *vp);
    static bool findAllGlobals(JSContext *cx, unsigned argc, Value *vp);
    static bool makeGlobalObjectReference(JSContext *cx, unsigned argc, Value *vp);
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];

    JSObject *getHook(Hook hook) const;
    bool hasAnyLiveHooks() const;

    static JSTrapStatus slowPathOnEnterFrame(JSContext *cx, AbstractFramePtr frame,
                                             MutableHandleValue vp);
    static bool slowPathOnLeaveFrame(JSContext *cx, AbstractFramePtr frame, bool ok);
    static void slowPathOnNewScript(JSContext *cx, HandleScript script,
                                    GlobalObject *compileAndGoGlobal);
    static void slowPathOnNewGlobalObject(JSContext *cx, Handle<GlobalObject *> global);
    static JSTrapStatus dispatchHook(JSContext *cx, MutableHandleValue vp, Hook which);

    JSTrapStatus fireDebuggerStatement(JSContext *cx, MutableHandleValue vp);
    JSTrapStatus fireExceptionUnwind(JSContext *cx, MutableHandleValue vp);
    JSTrapStatus fireEnterFrame(JSContext *cx, AbstractFramePtr frame, MutableHandleValue vp);
    JSTrapStatus fireNewGlobalObject(JSContext *cx, Handle<GlobalObject *> global, MutableHandleValue vp);

    /*
     * Allocate and initialize a Debugger.Script instance whose referent is
     * |script|.
     */
    JSObject *newDebuggerScript(JSContext *cx, HandleScript script);

    /*
     * Allocate and initialize a Debugger.Source instance whose referent is
     * |source|.
     */
    JSObject *newDebuggerSource(JSContext *cx, js::HandleScriptSource source);

    /*
     * Receive a "new script" event from the engine. A new script was compiled
     * or deserialized.
     */
    void fireNewScript(JSContext *cx, HandleScript script);

    inline Breakpoint *firstBreakpoint() const;

    static inline Debugger *fromOnNewGlobalObjectWatchersLink(JSCList *link);

  public:
    Debugger(JSContext *cx, JSObject *dbg);
    ~Debugger();

    bool init(JSContext *cx);
    inline const js::HeapPtrObject &toJSObject() const;
    inline js::HeapPtrObject &toJSObjectRef();
    static inline Debugger *fromJSObject(JSObject *obj);
    static Debugger *fromChildJSObject(JSObject *obj);

    /*********************************** Methods for interaction with the GC. */

    /*
     * A Debugger object is live if:
     *   * the Debugger JSObject is live (Debugger::trace handles this case); OR
     *   * it is in the middle of dispatching an event (the event dispatching
     *     code roots it in this case); OR
     *   * it is enabled, and it is debugging at least one live compartment,
     *     and at least one of the following is true:
     *       - it has a debugger hook installed
     *       - it has a breakpoint set on a live script
     *       - it has a watchpoint set on a live object.
     *
     * Debugger::markAllIteratively handles the last case. If it finds any
     * Debugger objects that are definitely live but not yet marked, it marks
     * them and returns true. If not, it returns false.
     */
    static void markCrossCompartmentDebuggerObjectReferents(JSTracer *tracer);
    static bool markAllIteratively(GCMarker *trc);
    static void markAll(JSTracer *trc);
    static void sweepAll(FreeOp *fop);
    static void detachAllDebuggersFromGlobal(FreeOp *fop, GlobalObject *global,
                                             GlobalObjectSet::Enum *compartmentEnum);
    static void findCompartmentEdges(JS::Zone *v, gc::ComponentFinder<JS::Zone> &finder);

    static inline JSTrapStatus onEnterFrame(JSContext *cx, AbstractFramePtr frame,
                                            MutableHandleValue vp);
    static inline bool onLeaveFrame(JSContext *cx, AbstractFramePtr frame, bool ok);
    static inline JSTrapStatus onDebuggerStatement(JSContext *cx, MutableHandleValue vp);
    static inline JSTrapStatus onExceptionUnwind(JSContext *cx, MutableHandleValue vp);
    static inline void onNewScript(JSContext *cx, HandleScript script,
                                   GlobalObject *compileAndGoGlobal);
    static inline void onNewGlobalObject(JSContext *cx, Handle<GlobalObject *> global);
    static JSTrapStatus onTrap(JSContext *cx, MutableHandleValue vp);
    static JSTrapStatus onSingleStep(JSContext *cx, MutableHandleValue vp);
    static bool handleBaselineOsr(JSContext *cx, StackFrame *from, jit::BaselineFrame *to);

    /************************************* Functions for use by Debugger.cpp. */

    inline bool observesEnterFrame() const;
    inline bool observesNewScript() const;
    inline bool observesNewGlobalObject() const;
    inline bool observesGlobal(GlobalObject *global) const;
    bool observesFrame(AbstractFramePtr frame) const;
    bool observesScript(JSScript *script) const;

    /*
     * If env is nullptr, call vp->setNull() and return true. Otherwise, find
     * or create a Debugger.Environment object for the given Env. On success,
     * store the Environment object in *vp and return true.
     */
    bool wrapEnvironment(JSContext *cx, Handle<Env*> env, MutableHandleValue vp);

    /*
     * Like cx->compartment()->wrap(cx, vp), but for the debugger compartment.
     *
     * Preconditions: *vp is a value from a debuggee compartment; cx is in the
     * debugger's compartment.
     *
     * If *vp is an object, this produces a (new or existing) Debugger.Object
     * wrapper for it. Otherwise this is the same as JSCompartment::wrap.
     */
    bool wrapDebuggeeValue(JSContext *cx, MutableHandleValue vp);

    /*
     * Unwrap a Debug.Object, without rewrapping it for any particular debuggee
     * compartment.
     *
     * Preconditions: cx is in the debugger compartment. *vp is a value in that
     * compartment. (*vp should be a "debuggee value", meaning it is the
     * debugger's reflection of a value in the debuggee.)
     *
     * If *vp is a Debugger.Object, store the referent in *vp. Otherwise, if *vp
     * is an object, throw a TypeError, because it is not a debuggee
     * value. Otherwise *vp is a primitive, so leave it alone.
     *
     * When passing values from the debuggee to the debugger:
     *     enter debugger compartment;
     *     call wrapDebuggeeValue;  // compartment- and debugger-wrapping
     *
     * When passing values from the debugger to the debuggee:
     *     call unwrapDebuggeeValue;  // debugger-unwrapping
     *     enter debuggee compartment;
     *     call cx->compartment()->wrap;  // compartment-rewrapping
     *
     * (Extreme nerd sidebar: Unwrapping happens in two steps because there are
     * two different kinds of symmetry at work: regardless of which direction
     * we're going, we want any exceptions to be created and thrown in the
     * debugger compartment--mirror symmetry. But compartment wrapping always
     * happens in the target compartment--rotational symmetry.)
     */
    bool unwrapDebuggeeValue(JSContext *cx, MutableHandleValue vp);

    /*
     * Store the Debugger.Frame object for frame in *vp.
     *
     * Use this if you have already access to a frame pointer without having
     * to incur the cost of walking the stack.
     */
    bool getScriptFrame(JSContext *cx, AbstractFramePtr frame, MutableHandleValue vp);

    /*
     * Store the Debugger.Frame object for iter in *vp. Eagerly copies a
     * ScriptFrameIter::Data.
     *
     * Use this if you had to make a ScriptFrameIter to get the required
     * frame, in which case the cost of walking the stack has already been
     * paid.
     */
    bool getScriptFrame(JSContext *cx, const ScriptFrameIter &iter, MutableHandleValue vp) {
        AbstractFramePtr data = iter.copyDataAsAbstractFramePtr();
        if (!data || !getScriptFrame(cx, iter.abstractFramePtr(), vp))
            return false;
        vp.toObject().setPrivate(data.raw());
        return true;
    }

    /*
     * Set |*status| and |*value| to a (JSTrapStatus, Value) pair reflecting a
     * standard SpiderMonkey call state: a boolean success value |ok|, a return
     * value |rv|, and a context |cx| that may or may not have an exception set.
     * If an exception was pending on |cx|, it is cleared (and |ok| is asserted
     * to be false).
     */
    static void resultToCompletion(JSContext *cx, bool ok, const Value &rv,
                                   JSTrapStatus *status, MutableHandleValue value);

    /*
     * Set |*result| to a JavaScript completion value corresponding to |status|
     * and |value|. |value| should be the return value or exception value, not
     * wrapped as a debuggee value. |cx| must be in the debugger compartment.
     */
    bool newCompletionValue(JSContext *cx, JSTrapStatus status, Value value,
                            MutableHandleValue result);

    /*
     * Precondition: we are in the debuggee compartment (ac is entered) and ok
     * is true if the operation in the debuggee compartment succeeded, false on
     * error or exception.
     *
     * Postcondition: we are in the debugger compartment, having called
     * ac.leave() even if an error occurred.
     *
     * On success, a completion value is in vp and ac.context does not have a
     * pending exception. (This ordinarily returns true even if the ok argument
     * is false.)
     */
    bool receiveCompletionValue(mozilla::Maybe<AutoCompartment> &ac, bool ok, Value val,
                                MutableHandleValue vp);

    /*
     * Return the Debugger.Script object for |script|, or create a new one if
     * needed. The context |cx| must be in the debugger compartment; |script|
     * must be a script in a debuggee compartment.
     */
    JSObject *wrapScript(JSContext *cx, HandleScript script);

    /*
     * Return the Debugger.Source object for |source|, or create a new one if
     * needed. The context |cx| must be in the debugger compartment; |source|
     * must be a script source object in a debuggee compartment.
     */
    JSObject *wrapSource(JSContext *cx, js::HandleScriptSource source);

  private:
    Debugger(const Debugger &) MOZ_DELETE;
    Debugger & operator=(const Debugger &) MOZ_DELETE;
};

class BreakpointSite {
    friend class Breakpoint;
    friend struct ::JSCompartment;
    friend class ::JSScript;
    friend class Debugger;

  public:
    JSScript *script;
    jsbytecode * const pc;

  private:
    JSCList breakpoints;  /* cyclic list of all js::Breakpoints at this instruction */
    size_t enabledCount;  /* number of breakpoints in the list that are enabled */
    JSTrapHandler trapHandler;  /* trap state */
    HeapValue trapClosure;

    void recompile(FreeOp *fop);

  public:
    BreakpointSite(JSScript *script, jsbytecode *pc);
    Breakpoint *firstBreakpoint() const;
    bool hasBreakpoint(Breakpoint *bp);
    bool hasTrap() const { return !!trapHandler; }

    void inc(FreeOp *fop);
    void dec(FreeOp *fop);
    void setTrap(FreeOp *fop, JSTrapHandler handler, const Value &closure);
    void clearTrap(FreeOp *fop, JSTrapHandler *handlerp = nullptr, Value *closurep = nullptr);
    void destroyIfEmpty(FreeOp *fop);
};

/*
 * Each Breakpoint is a member of two linked lists: its debugger's list and its
 * site's list.
 *
 * GC rules:
 *   - script is live, breakpoint exists, and debugger is enabled
 *      ==> debugger is live
 *   - script is live, breakpoint exists, and debugger is live
 *      ==> retain the breakpoint and the handler object is live
 *
 * Debugger::markAllIteratively implements these two rules. It uses
 * Debugger::hasAnyLiveHooks to check for rule 1.
 *
 * Nothing else causes a breakpoint to be retained, so if its script or
 * debugger is collected, the breakpoint is destroyed during GC sweep phase,
 * even if the debugger compartment isn't being GC'd. This is implemented in
 * JSCompartment::sweepBreakpoints.
 */
class Breakpoint {
    friend struct ::JSCompartment;
    friend class Debugger;

  public:
    Debugger * const debugger;
    BreakpointSite * const site;
  private:
    /* |handler| is marked unconditionally during minor GC. */
    js::EncapsulatedPtrObject handler;
    JSCList debuggerLinks;
    JSCList siteLinks;

  public:
    static Breakpoint *fromDebuggerLinks(JSCList *links);
    static Breakpoint *fromSiteLinks(JSCList *links);
    Breakpoint(Debugger *debugger, BreakpointSite *site, JSObject *handler);
    void destroy(FreeOp *fop);
    Breakpoint *nextInDebugger();
    Breakpoint *nextInSite();
    const EncapsulatedPtrObject &getHandler() const { return handler; }
    EncapsulatedPtrObject &getHandlerRef() { return handler; }
};

Breakpoint *
Debugger::firstBreakpoint() const
{
    if (JS_CLIST_IS_EMPTY(&breakpoints))
        return nullptr;
    return Breakpoint::fromDebuggerLinks(JS_NEXT_LINK(&breakpoints));
}

Debugger *
Debugger::fromOnNewGlobalObjectWatchersLink(JSCList *link) {
    char *p = reinterpret_cast<char *>(link);
    return reinterpret_cast<Debugger *>(p - offsetof(Debugger, onNewGlobalObjectWatchersLink));
}

const js::HeapPtrObject &
Debugger::toJSObject() const
{
    JS_ASSERT(object);
    return object;
}

js::HeapPtrObject &
Debugger::toJSObjectRef()
{
    JS_ASSERT(object);
    return object;
}

bool
Debugger::observesEnterFrame() const
{
    return enabled && getHook(OnEnterFrame);
}

bool
Debugger::observesNewScript() const
{
    return enabled && getHook(OnNewScript);
}

bool
Debugger::observesNewGlobalObject() const
{
    return enabled && getHook(OnNewGlobalObject);
}

bool
Debugger::observesGlobal(GlobalObject *global) const
{
    return debuggees.has(global);
}

JSTrapStatus
Debugger::onEnterFrame(JSContext *cx, AbstractFramePtr frame, MutableHandleValue vp)
{
    if (cx->compartment()->getDebuggees().empty())
        return JSTRAP_CONTINUE;
    return slowPathOnEnterFrame(cx, frame, vp);
}

JSTrapStatus
Debugger::onDebuggerStatement(JSContext *cx, MutableHandleValue vp)
{
    return cx->compartment()->getDebuggees().empty()
           ? JSTRAP_CONTINUE
           : dispatchHook(cx, vp, OnDebuggerStatement);
}

JSTrapStatus
Debugger::onExceptionUnwind(JSContext *cx, MutableHandleValue vp)
{
    return cx->compartment()->getDebuggees().empty()
           ? JSTRAP_CONTINUE
           : dispatchHook(cx, vp, OnExceptionUnwind);
}

void
Debugger::onNewScript(JSContext *cx, HandleScript script, GlobalObject *compileAndGoGlobal)
{
    JS_ASSERT_IF(script->compileAndGo, compileAndGoGlobal);
    JS_ASSERT_IF(script->compileAndGo, compileAndGoGlobal == &script->uninlinedGlobal());
    // We early return in slowPathOnNewScript for self-hosted scripts, so we can
    // ignore those in our assertion here.
    JS_ASSERT_IF(!script->compartment()->options().invisibleToDebugger() &&
                 !script->selfHosted,
                 script->compartment()->firedOnNewGlobalObject);
    JS_ASSERT_IF(!script->compileAndGo, !compileAndGoGlobal);
    if (!script->compartment()->getDebuggees().empty())
        slowPathOnNewScript(cx, script, compileAndGoGlobal);
}

void
Debugger::onNewGlobalObject(JSContext *cx, Handle<GlobalObject *> global)
{
    JS_ASSERT(!global->compartment()->firedOnNewGlobalObject);
#ifdef DEBUG
    global->compartment()->firedOnNewGlobalObject = true;
#endif
    if (!JS_CLIST_IS_EMPTY(&cx->runtime()->onNewGlobalObjectWatchers))
        Debugger::slowPathOnNewGlobalObject(cx, global);
}

extern bool
EvaluateInEnv(JSContext *cx, Handle<Env*> env, HandleValue thisv, AbstractFramePtr frame,
              StableCharPtr chars, unsigned length, const char *filename, unsigned lineno,
              MutableHandleValue rval);

}

#endif /* vm_Debugger_h */
