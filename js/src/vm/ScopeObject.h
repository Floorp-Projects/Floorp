/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ScopeObject_h
#define vm_ScopeObject_h

#include "jscntxt.h"
#include "jsobj.h"
#include "jsweakmap.h"

#include "gc/Barrier.h"

namespace js {

/*****************************************************************************/

/*
 * All function scripts have an "enclosing static scope" that refers to the
 * innermost enclosing let or function in the program text. This allows full
 * reconstruction of the lexical scope for debugging or compiling efficient
 * access to variables in enclosing scopes. The static scope is represented at
 * runtime by a tree of compiler-created objects representing each scope:
 *  - a StaticBlockObject is created for 'let' and 'catch' scopes
 *  - a JSFunction+JSScript+Bindings trio is created for function scopes
 * (These objects are primarily used to clone objects scopes for the
 * dynamic scope chain.)
 *
 * There is an additional scope for named lambdas. E.g., in:
 *
 *   (function f() { var x; function g() { } })
 *
 * g's innermost enclosing scope will first be the function scope containing
 * 'x', enclosed by a scope containing only the name 'f'. (This separate scope
 * is necessary due to the fact that declarations in the function scope shadow
 * (dynamically, in the case of 'eval') the lambda name.)
 *
 * There are two limitations to the current lexical nesting information:
 *
 *  - 'with' is completely absent; this isn't a problem for the current use
 *    cases since 'with' causes every static scope to be on the dynamic scope
 *    chain (so the debugger can find everything) and inhibits all upvar
 *    optimization.
 *
 *  - The "enclosing static scope" chain stops at 'eval'. For example in:
 *      let (x) { eval("function f() {}") }
 *    f does not have an enclosing static scope. This is fine for current uses
 *    for the same reason as 'with'.
 *
 * (See also AssertDynamicScopeMatchesStaticScope.)
 */
class StaticScopeIter
{
    RootedObject obj;
    bool onNamedLambda;

  public:
    explicit StaticScopeIter(JSContext *cx, JSObject *obj);

    bool done() const;
    void operator++(int);

    /* Return whether this static scope will be on the dynamic scope chain. */
    bool hasDynamicScopeObject() const;
    Shape *scopeShape() const;

    enum Type { BLOCK, FUNCTION, NAMED_LAMBDA };
    Type type() const;

    StaticBlockObject &block() const;
    JSScript *funScript() const;
};

/*****************************************************************************/

/*
 * A "scope coordinate" describes how to get from head of the scope chain to a
 * given lexically-enclosing variable. A scope coordinate has two dimensions:
 *  - hops: the number of scope objects on the scope chain to skip
 *  - slot: the slot on the scope object holding the variable's value
 * Additionally (as described in jsopcode.tbl) there is a 'block' index, but
 * this is only needed for decompilation/inference so it is not included in the
 * main ScopeCoordinate struct: use ScopeCoordinate{BlockChain,Name} instead.
 */
struct ScopeCoordinate
{
    uint16_t hops;
    uint16_t slot;

    inline ScopeCoordinate(jsbytecode *pc);
    inline ScopeCoordinate() {}
};

/*
 * Return a shape representing the static scope containing the variable
 * accessed by the ALIASEDVAR op at 'pc'.
 */
extern Shape *
ScopeCoordinateToStaticScopeShape(JSContext *cx, JSScript *script, jsbytecode *pc);

/* Return the name being accessed by the given ALIASEDVAR op. */
extern PropertyName *
ScopeCoordinateName(JSContext *cx, JSScript *script, jsbytecode *pc);

/* Return the function script accessed by the given ALIASEDVAR op, or NULL. */
extern JSScript *
ScopeCoordinateFunctionScript(JSContext *cx, JSScript *script, jsbytecode *pc);

/*****************************************************************************/

/*
 * Scope objects
 *
 * Scope objects are technically real JSObjects but only belong on the scope
 * chain (that is, fp->scopeChain() or fun->environment()). The hierarchy of
 * scope objects is:
 *
 *   JSObject                   Generic object
 *     \
 *   ScopeObject                Engine-internal scope
 *     \   \   \
 *      \   \  DeclEnvObject    Holds name of recursive/heavyweight named lambda
 *       \   \
 *        \  CallObject         Scope of entire function or strict eval
 *         \
 *   NestedScopeObject          Scope created for a statement
 *     \   \
 *      \  WithObject           with
 *       \
 *   BlockObject                Shared interface of cloned/static block objects
 *     \   \
 *      \  ClonedBlockObject    let, switch, catch, for
 *       \
 *       StaticBlockObject      See NB
 *
 * This hierarchy represents more than just the interface hierarchy: reserved
 * slots in base classes are fixed for all derived classes. Thus, for example,
 * ScopeObject::enclosingScope() can simply access a fixed slot without further
 * dynamic type information.
 *
 * NB: Static block objects are a special case: these objects are created at
 * compile time to hold the shape/binding information from which block objects
 * are cloned at runtime. These objects should never escape into the wild and
 * support a restricted set of ScopeObject operations.
 *
 * See also "Debug scope objects" below.
 */

class ScopeObject : public JSObject
{
  protected:
    static const uint32_t SCOPE_CHAIN_SLOT = 0;

  public:
    /*
     * Since every scope chain terminates with a global object and GlobalObject
     * does not derive ScopeObject (it has a completely different layout), the
     * enclosing scope of a ScopeObject is necessarily non-null.
     */
    inline JSObject &enclosingScope() const {
        return getReservedSlot(SCOPE_CHAIN_SLOT).toObject();
    }
    inline void setEnclosingScope(HandleObject obj);

    /*
     * Get or set an aliased variable contained in this scope. Unaliased
     * variables should instead access the StackFrame. Aliased variable access
     * is primarily made through JOF_SCOPECOORD ops which is why these members
     * take a ScopeCoordinate instead of just the slot index.
     */
    inline const Value &aliasedVar(ScopeCoordinate sc);
    inline void setAliasedVar(JSContext *cx, ScopeCoordinate sc, PropertyName *name, const Value &v);

    /* For jit access. */
    static inline size_t offsetOfEnclosingScope();

    static inline size_t enclosingScopeSlot() {
        return SCOPE_CHAIN_SLOT;
    }
};

class CallObject : public ScopeObject
{
    static const uint32_t CALLEE_SLOT = 1;

    static CallObject *
    create(JSContext *cx, HandleScript script, HandleObject enclosing, HandleFunction callee);

  public:
    static Class class_;

    /* These functions are internal and are exposed only for JITs. */
    static CallObject *
    create(JSContext *cx, HandleScript script, HandleShape shape, HandleTypeObject type, HeapSlot *slots);

    static CallObject *
    createTemplateObject(JSContext *cx, HandleScript script, gc::InitialHeap heap);

    static const uint32_t RESERVED_SLOTS = 2;

    static CallObject *createForFunction(JSContext *cx, HandleObject enclosing, HandleFunction callee);

    static CallObject *createForFunction(JSContext *cx, AbstractFramePtr frame);
    static CallObject *createForStrictEval(JSContext *cx, AbstractFramePtr frame);

    /* True if this is for a strict mode eval frame. */
    inline bool isForEval() const;

    /*
     * Returns the function for which this CallObject was created. (This may
     * only be called if !isForEval.)
     */
    inline JSFunction &callee() const;

    /* Get/set the aliased variable referred to by 'bi'. */
    inline const Value &aliasedVar(AliasedFormalIter fi);
    inline void setAliasedVar(JSContext *cx, AliasedFormalIter fi, PropertyName *name, const Value &v);

    /* For jit access. */
    static inline size_t offsetOfCallee();
    static inline size_t calleeSlot() {
        return CALLEE_SLOT;
    }
};

class DeclEnvObject : public ScopeObject
{
    // Pre-allocated slot for the named lambda.
    static const uint32_t LAMBDA_SLOT = 1;

  public:
    static const uint32_t RESERVED_SLOTS = 2;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT2;

    static Class class_;

    static DeclEnvObject *
    createTemplateObject(JSContext *cx, HandleFunction fun, gc::InitialHeap heap);

    static DeclEnvObject *create(JSContext *cx, HandleObject enclosing, HandleFunction callee);

    static inline size_t lambdaSlot() {
        return LAMBDA_SLOT;
    }
};

class NestedScopeObject : public ScopeObject
{
  protected:
    static const unsigned DEPTH_SLOT = 1;

  public:
    /* Return the abstract stack depth right before entering this nested scope. */
    uint32_t stackDepth() const;
};

class WithObject : public NestedScopeObject
{
    static const unsigned THIS_SLOT = 2;

    /* Use WithObject::object() instead. */
    JSObject *getProto() const;

  public:
    static const unsigned RESERVED_SLOTS = 3;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;

    static Class class_;

    static WithObject *
    create(JSContext *cx, HandleObject proto, HandleObject enclosing, uint32_t depth);

    /* Return object for the 'this' class hook. */
    JSObject &withThis() const;

    /* Return the 'o' in 'with (o)'. */
    JSObject &object() const;
};

class BlockObject : public NestedScopeObject
{
  public:
    static const unsigned RESERVED_SLOTS = 2;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;

    static Class class_;

    /* Return the number of variables associated with this block. */
    inline uint32_t slotCount() const;

    /*
     * Return the local corresponding to the ith binding where i is in the
     * range [0, slotCount()) and the return local index is in the range
     * [script->nfixed, script->nfixed + script->nslots).
     */
    unsigned slotToLocalIndex(const Bindings &bindings, unsigned slot);
    unsigned localIndexToSlot(const Bindings &bindings, uint32_t i);

  protected:
    /* Blocks contain an object slot for each slot i: 0 <= i < slotCount. */
    inline const Value &slotValue(unsigned i);
    inline void setSlotValue(unsigned i, const Value &v);
};

class StaticBlockObject : public BlockObject
{
  public:
    static StaticBlockObject *create(JSContext *cx);

    /* See StaticScopeIter comment. */
    inline JSObject *enclosingStaticScope() const;

    /*
     * A refinement of enclosingStaticScope that returns NULL if the enclosing
     * static scope is a JSFunction.
     */
    inline StaticBlockObject *enclosingBlock() const;

    /*
     * Return whether this StaticBlockObject contains a variable stored at
     * the given stack depth (i.e., fp->base()[depth]).
     */
    bool containsVarAtDepth(uint32_t depth);

    /*
     * A let binding is aliased if accessed lexically by nested functions or
     * dynamically through dynamic name lookup (eval, with, function::, etc).
     */
    bool isAliased(unsigned i);

    /*
     * A static block object is cloned (when entering the block) iff some
     * variable of the block isAliased.
     */
    bool needsClone();

    /* Frontend-only functions ***********************************************/

    /* Initialization functions for above fields. */
    void setAliased(unsigned i, bool aliased);
    void setStackDepth(uint32_t depth);
    void initEnclosingStaticScope(JSObject *obj);

    /*
     * Frontend compilation temporarily uses the object's slots to link
     * a let var to its associated Definition parse node.
     */
    void setDefinitionParseNode(unsigned i, frontend::Definition *def);
    frontend::Definition *maybeDefinitionParseNode(unsigned i);

    /*
     * The parser uses 'enclosingBlock' as the prev-link in the pc->blockChain
     * stack. Note: in the case of hoisting, this prev-link will not ultimately
     * be the same as enclosingBlock, initEnclosingStaticScope must be called
     * separately in the emitter. 'reset' is just for asserting stackiness.
     */
    void initPrevBlockChainFromParser(StaticBlockObject *prev);
    void resetPrevBlockChainFromParser();

    static Shape *addVar(JSContext *cx, Handle<StaticBlockObject*> block, HandleId id,
                           int index, bool *redeclared);
};

class ClonedBlockObject : public BlockObject
{
  public:
    static ClonedBlockObject *create(JSContext *cx, Handle<StaticBlockObject *> block,
                                     AbstractFramePtr frame);

    /* The static block from which this block was cloned. */
    StaticBlockObject &staticBlock() const;

    /* Assuming 'put' has been called, return the value of the ith let var. */
    const Value &var(unsigned i, MaybeCheckAliasing = CHECK_ALIASING);
    void setVar(unsigned i, const Value &v, MaybeCheckAliasing = CHECK_ALIASING);

    /* Copy in all the unaliased formals and locals. */
    void copyUnaliasedValues(AbstractFramePtr frame);
};

template<XDRMode mode>
bool
XDRStaticBlockObject(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript script,
                     StaticBlockObject **objp);

extern JSObject *
CloneStaticBlockObject(JSContext *cx, HandleObject enclosingScope, Handle<StaticBlockObject*> src);

/*****************************************************************************/

class ScopeIterKey;

/*
 * A scope iterator describes the active scopes enclosing the current point of
 * execution for a single frame, proceeding from inner to outer. Here, "frame"
 * means a single activation of: a function, eval, or global code. By design,
 * ScopeIter exposes *all* scopes, even those that have been optimized away
 * (i.e., no ScopeObject was created when entering the scope and thus there is
 * no ScopeObject on fp->scopeChain representing the scope).
 *
 * Note: ScopeIter iterates over all scopes *within* a frame which means that
 * all scopes are ScopeObjects. In particular, the GlobalObject enclosing
 * global code (and any random objects passed as scopes to Execute) will not
 * be included.
 */
class ScopeIter
{
    friend class ScopeIterKey;

  public:
    enum Type { Call, Block, With, StrictEvalScope };

  private:
    JSContext *cx;
    AbstractFramePtr frame_;
    RootedObject cur_;
    Rooted<StaticBlockObject *> block_;
    Type type_;
    bool hasScopeObject_;

    void settle();

    /* ScopeIter does not have value semantics. */
    ScopeIter(const ScopeIter &si) MOZ_DELETE;

  public:
    /* The default constructor leaves ScopeIter totally invalid */
    explicit ScopeIter(JSContext *cx
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /* Constructing from a copy of an existing ScopeIter. */
    explicit ScopeIter(const ScopeIter &si, JSContext *cx
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /* Constructing from StackFrame places ScopeIter on the innermost scope. */
    explicit ScopeIter(AbstractFramePtr frame, JSContext *cx
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /*
     * Without a StackFrame, the resulting ScopeIter is done() with
     * enclosingScope() as given.
     */
    explicit ScopeIter(JSObject &enclosingScope, JSContext *cx
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /*
     * For the special case of generators, copy the given ScopeIter, with 'fp'
     * as the StackFrame instead of si.fp(). Not for general use.
     */
    ScopeIter(const ScopeIter &si, AbstractFramePtr frame, JSContext *cx
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /* Like ScopeIter(StackFrame *) except start at 'scope'. */
    ScopeIter(AbstractFramePtr frame, ScopeObject &scope, JSContext *cx
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    bool done() const { return !frame_; }

    /* If done(): */

    JSObject &enclosingScope() const { JS_ASSERT(done()); return *cur_; }

    /* If !done(): */

    ScopeIter &operator++();

    AbstractFramePtr frame() const { JS_ASSERT(!done()); return frame_; }
    Type type() const { JS_ASSERT(!done()); return type_; }
    bool hasScopeObject() const { JS_ASSERT(!done()); return hasScopeObject_; }
    ScopeObject &scope() const;

    StaticBlockObject &staticBlock() const { JS_ASSERT(type() == Block); return *block_; }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class ScopeIterKey
{
    AbstractFramePtr frame_;
    JSObject *cur_;
    StaticBlockObject *block_;
    ScopeIter::Type type_;

  public:
    ScopeIterKey() : frame_(NullFramePtr()), cur_(NULL), block_(NULL), type_() {}
    ScopeIterKey(const ScopeIter &si)
      : frame_(si.frame_), cur_(si.cur_), block_(si.block_), type_(si.type_)
    {}

    AbstractFramePtr frame() const { return frame_; }
    ScopeIter::Type type() const { return type_; }

    /* For use as hash policy */
    typedef ScopeIterKey Lookup;
    static HashNumber hash(ScopeIterKey si);
    static bool match(ScopeIterKey si1, ScopeIterKey si2);
};

/*****************************************************************************/

/*
 * Debug scope objects
 *
 * The debugger effectively turns every opcode into a potential direct eval.
 * Naively, this would require creating a ScopeObject for every call/block
 * scope and using JSOP_GETALIASEDVAR for every access. To optimize this, the
 * engine assumes there is no debugger and optimizes scope access and creation
 * accordingly. When the debugger wants to perform an unexpected eval-in-frame
 * (or other, similar dynamic-scope-requiring operations), fp->scopeChain is
 * now incomplete: it may not contain all, or any, of the ScopeObjects to
 * represent the current scope.
 *
 * To resolve this, the debugger first calls GetDebugScopeFor(Function|Frame)
 * to synthesize a "debug scope chain". A debug scope chain is just a chain of
 * objects that fill in missing scopes and protect the engine from unexpected
 * access. (The latter means that some debugger operations, like redefining a
 * lexical binding, can fail when a true eval would succeed.) To do both of
 * these things, GetDebugScopeFor* creates a new proxy DebugScopeObject to sit
 * in front of every existing ScopeObject.
 *
 * GetDebugScopeFor* ensures the invariant that the same DebugScopeObject is
 * always produced for the same underlying scope (optimized or not!). This is
 * maintained by some bookkeeping information stored in DebugScopes.
 */

extern JSObject *
GetDebugScopeForFunction(JSContext *cx, HandleFunction fun);

extern JSObject *
GetDebugScopeForFrame(JSContext *cx, AbstractFramePtr frame);

/* Provides debugger access to a scope. */
class DebugScopeObject : public JSObject
{
    /*
     * The enclosing scope on the dynamic scope chain. This slot is analogous
     * to the SCOPE_CHAIN_SLOT of a ScopeObject.
     */
    static const unsigned ENCLOSING_EXTRA = 0;

    /*
     * NullValue or a dense array holding the unaliased variables of a function
     * frame that has been popped.
     */
    static const unsigned SNAPSHOT_EXTRA = 1;

  public:
    static DebugScopeObject *create(JSContext *cx, ScopeObject &scope, HandleObject enclosing);

    ScopeObject &scope() const;
    JSObject &enclosingScope() const;

    /* May only be called for proxies to function call objects. */
    JSObject *maybeSnapshot() const;
    void initSnapshot(JSObject &snapshot);

    /* Currently, the 'declarative' scopes are Call and Block. */
    bool isForDeclarative() const;
};

/* Maintains per-compartment debug scope bookkeeping information. */
class DebugScopes
{
    /* The map from (non-debug) scopes to debug scopes. */
    typedef WeakMap<EncapsulatedPtrObject, RelocatablePtrObject> ObjectWeakMap;
    ObjectWeakMap proxiedScopes;

    /*
     * The map from live frames which have optimized-away scopes to the
     * corresponding debug scopes.
     */
    typedef HashMap<ScopeIterKey,
                    ReadBarriered<DebugScopeObject>,
                    ScopeIterKey,
                    RuntimeAllocPolicy> MissingScopeMap;
    MissingScopeMap missingScopes;

    /*
     * The map from scope objects of live frames to the live frame. This map
     * updated lazily whenever the debugger needs the information. In between
     * two lazy updates, liveScopes becomes incomplete (but not invalid, onPop*
     * removes scopes as they are popped). Thus, two consecutive debugger lazy
     * updates of liveScopes need only fill in the new scopes.
     */
    typedef HashMap<ScopeObject *,
                    AbstractFramePtr,
                    DefaultHasher<ScopeObject *>,
                    RuntimeAllocPolicy> LiveScopeMap;
    LiveScopeMap liveScopes;

  public:
    DebugScopes(JSContext *c);
    ~DebugScopes();

  private:
    bool init();

    static DebugScopes *ensureCompartmentData(JSContext *cx);

  public:
    void mark(JSTracer *trc);
    void sweep(JSRuntime *rt);

    static DebugScopeObject *hasDebugScope(JSContext *cx, ScopeObject &scope);
    static bool addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope);

    static DebugScopeObject *hasDebugScope(JSContext *cx, const ScopeIter &si);
    static bool addDebugScope(JSContext *cx, const ScopeIter &si, DebugScopeObject &debugScope);

    static bool updateLiveScopes(JSContext *cx);
    static AbstractFramePtr hasLiveFrame(ScopeObject &scope);

    /*
     * In debug-mode, these must be called whenever exiting a call/block or
     * when activating/yielding a generator.
     */
    static void onPopCall(AbstractFramePtr frame, JSContext *cx);
    static void onPopBlock(JSContext *cx, AbstractFramePtr frame);
    static void onPopWith(AbstractFramePtr frame);
    static void onPopStrictEvalScope(AbstractFramePtr frame);
    static void onGeneratorFrameChange(AbstractFramePtr from, AbstractFramePtr to, JSContext *cx);
    static void onCompartmentLeaveDebugMode(JSCompartment *c);
};

}  /* namespace js */

template<>
inline bool
JSObject::is<js::NestedScopeObject>() const
{
    return is<js::BlockObject>() || is<js::WithObject>();
}

template<>
inline bool
JSObject::is<js::ScopeObject>() const
{
    return is<js::CallObject>() || is<js::DeclEnvObject>() || is<js::NestedScopeObject>();
}

template<>
inline bool
JSObject::is<js::DebugScopeObject>() const
{
    extern bool js_IsDebugScopeSlow(JSObject *obj);
    return getClass() == &js::ObjectProxyClass && js_IsDebugScopeSlow(const_cast<JSObject*>(this));
}

#endif /* vm_ScopeObject_h */
