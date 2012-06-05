/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScopeObject_h___
#define ScopeObject_h___

#include "jscntxt.h"
#include "jsobj.h"
#include "jsweakmap.h"

namespace js {

/*****************************************************************************/

/*
 * A "scope coordinate" describes how to get from head of the scope chain to a
 * given lexically-enclosing variable. A scope coordinate has two dimensions:
 *  - hops: the number of scope objects on the scope chain to skip
 *  - binding: which binding on the scope object
 * Additionally (as described in jsopcode.tbl) there is a 'block' index, but
 * this is only needed for decompilation/inference so it is not included in the
 * main ScopeCoordinate struct: use ScopeCoordinate{BlockChain,Atom} instead.
 */
struct ScopeCoordinate
{
    uint16_t hops;
    uint16_t slot;

    inline ScopeCoordinate(jsbytecode *pc);
    inline ScopeCoordinate() {}
};

/* Return the static block chain (or null) accessed by *pc. */
extern StaticBlockObject *
ScopeCoordinateBlockChain(JSScript *script, jsbytecode *pc);

/* Return the name being accessed by the given ALIASEDVAR op. */
extern PropertyName *
ScopeCoordinateName(JSRuntime *rt, JSScript *script, jsbytecode *pc);

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
    /* Number of reserved slots for both CallObject and BlockObject. */
    static const uint32_t CALL_BLOCK_RESERVED_SLOTS = 2;

    /*
     * Since every scope chain terminates with a global object and GlobalObject
     * does not derive ScopeObject (it has a completely different layout), the
     * enclosing scope of a ScopeObject is necessarily non-null.
     */
    inline JSObject &enclosingScope() const;
    inline bool setEnclosingScope(JSContext *cx, HandleObject obj);

    /*
     * Get or set an aliased variable contained in this scope. Unaliased
     * variables should instead access the StackFrame. Aliased variable access
     * is primarily made through JOF_SCOPECOORD ops which is why these members
     * take a ScopeCoordinate instead of just the slot index.
     */
    inline const Value &aliasedVar(ScopeCoordinate sc);
    inline void setAliasedVar(ScopeCoordinate sc, const Value &v);

    /* For jit access. */
    static inline size_t offsetOfEnclosingScope();
};

class CallObject : public ScopeObject
{
    static const uint32_t CALLEE_SLOT = 1;

    static CallObject *
    create(JSContext *cx, JSScript *script, HandleObject enclosing, HandleFunction callee);

  public:
    static const uint32_t RESERVED_SLOTS = CALL_BLOCK_RESERVED_SLOTS;

    static CallObject *createForFunction(JSContext *cx, StackFrame *fp);
    static CallObject *createForStrictEval(JSContext *cx, StackFrame *fp);

    /* True if this is for a strict mode eval frame or for a function call. */
    inline bool isForEval() const;

    /*
     * The callee function if this CallObject was created for a function
     * invocation, or null if it was created for a strict mode eval frame.
     */
    inline JSObject *getCallee() const;
    inline JSFunction *getCalleeFunction() const;
    inline void setCallee(JSObject *callee);

    /* Returns the formal argument at the given index. */
    inline const Value &arg(unsigned i, MaybeCheckAliasing = CHECK_ALIASING) const;
    inline void setArg(unsigned i, const Value &v, MaybeCheckAliasing = CHECK_ALIASING);

    /* Returns the variable at the given index. */
    inline const Value &var(unsigned i, MaybeCheckAliasing = CHECK_ALIASING) const;
    inline void setVar(unsigned i, const Value &v, MaybeCheckAliasing = CHECK_ALIASING);

    /*
     * Get the actual arrays of arguments and variables. Only call if type
     * inference is enabled, where we ensure that call object variables are in
     * contiguous slots (see NewCallObject).
     */
    inline HeapSlotArray argArray();
    inline HeapSlotArray varArray();

    static JSBool setArgOp(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp);
    static JSBool setVarOp(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp);

    /* Copy in all the unaliased formals and locals. */
    void copyUnaliasedValues(StackFrame *fp);
};

class DeclEnvObject : public ScopeObject
{
  public:
    static const uint32_t RESERVED_SLOTS = 1;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT2;

    static DeclEnvObject *create(JSContext *cx, StackFrame *fp);

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
#ifdef JS_THREADSAFE
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;
#else
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4;
#endif

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
    static const unsigned RESERVED_SLOTS = CALL_BLOCK_RESERVED_SLOTS;
#ifdef JS_THREADSAFE
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;
#else
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4;
#endif

    /* Return the number of variables associated with this block. */
    inline uint32_t slotCount() const;

    /*
     * Return the local corresponding to the ith binding where i is in the
     * range [0, slotCount()) and the return local index is in the range
     * [script->nfixed, script->nfixed + script->nslots).
     */
    unsigned slotToFrameLocal(JSScript *script, unsigned i);

  protected:
    /* Blocks contain an object slot for each slot i: 0 <= i < slotCount. */
    inline const Value &slotValue(unsigned i);
    inline void setSlotValue(unsigned i, const Value &v);
};

class StaticBlockObject : public BlockObject
{
  public:
    static StaticBlockObject *create(JSContext *cx);

    inline StaticBlockObject *enclosingBlock() const;
    inline void setEnclosingBlock(StaticBlockObject *blockObj);

    void setStackDepth(uint32_t depth);
    bool containsVarAtDepth(uint32_t depth);

    /*
     * Frontend compilation temporarily uses the object's slots to link
     * a let var to its associated Definition parse node.
     */
    void setDefinitionParseNode(unsigned i, Definition *def);
    Definition *maybeDefinitionParseNode(unsigned i);

    /*
     * A let binding is aliased is accessed lexically by nested functions or
     * dynamically through dynamic name lookup (eval, with, function::, etc).
     */
    void setAliased(unsigned i, bool aliased);
    bool isAliased(unsigned i);

    /*
     * A static block object is cloned (when entering the block) iff some
     * variable of the block isAliased.
     */
    bool needsClone();

    const Shape *addVar(JSContext *cx, jsid id, int index, bool *redeclared);
};

class ClonedBlockObject : public BlockObject
{
  public:
    static ClonedBlockObject *create(JSContext *cx, Handle<StaticBlockObject *> block,
                                     StackFrame *fp);

    /* The static block from which this block was cloned. */
    StaticBlockObject &staticBlock() const;

    /* Assuming 'put' has been called, return the value of the ith let var. */
    const Value &var(unsigned i, MaybeCheckAliasing = CHECK_ALIASING);
    void setVar(unsigned i, const Value &v, MaybeCheckAliasing = CHECK_ALIASING);

    /* Copy in all the unaliased formals and locals. */
    void copyUnaliasedValues(StackFrame *fp);
};

template<XDRMode mode>
bool
XDRStaticBlockObject(XDRState<mode> *xdr, JSScript *script, StaticBlockObject **objp);

extern JSObject *
CloneStaticBlockObject(JSContext *cx, StaticBlockObject &srcBlock,
                       const AutoObjectVector &objects, JSScript *src);

/*****************************************************************************/

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
  public:
    enum Type { Call, Block, With, StrictEvalScope };

  private:
    StackFrame *fp_;
    JSObject *cur_;
    StaticBlockObject *block_;
    Type type_;
    bool hasScopeObject_;

    void settle();

  public:
    /* The default constructor leaves ScopeIter totally invalid */
    explicit ScopeIter();

    /* Constructing from StackFrame places ScopeIter on the innermost scope. */
    explicit ScopeIter(StackFrame *fp);

    /*
     * Without a StackFrame, the resulting ScopeIter is done() with
     * enclosingScope() as given.
     */
    explicit ScopeIter(JSObject &enclosingScope);

    /*
     * For the special case of generators, copy the given ScopeIter, with 'fp'
     * as the StackFrame instead of si.fp(). Not for general use.
     */
    ScopeIter(ScopeIter si, StackFrame *fp);

    /* Like ScopeIter(StackFrame *) except start at 'scope'. */
    ScopeIter(StackFrame *fp, ScopeObject &scope);

    bool done() const { return !fp_; }

    /* If done(): */

    JSObject &enclosingScope() const { JS_ASSERT(done()); return *cur_; }

    /* If !done(): */

    ScopeIter enclosing() const;

    StackFrame *fp() const { JS_ASSERT(!done()); return fp_; }
    Type type() const { JS_ASSERT(!done()); return type_; }
    bool hasScopeObject() const { JS_ASSERT(!done()); return hasScopeObject_; }
    ScopeObject &scope() const;

    StaticBlockObject &staticBlock() const { JS_ASSERT(type() == Block); return *block_; }

    /* For use as hash policy */
    typedef ScopeIter Lookup;
    static HashNumber hash(ScopeIter si);
    static bool match(ScopeIter si1, ScopeIter si2);
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
 * access. (The latter means that some debugger operations, like adding a new
 * binding to a lexical scope, can fail when a true eval would succeed.) To do
 * both of these things, GetDebugScopeFor* creates a new proxy DebugScopeObject
 * to sit in front of every existing ScopeObject.
 *
 * GetDebugScopeFor* ensures the invariant that the same DebugScopeObject is
 * always produced for the same underlying scope (optimized or not!). This is
 * maintained by some bookkeeping information stored in DebugScopes.
 */

extern JSObject *
GetDebugScopeForFunction(JSContext *cx, JSFunction *fun);

extern JSObject *
GetDebugScopeForFrame(JSContext *cx, StackFrame *fp);

/* Provides debugger access to a scope. */
class DebugScopeObject : public JSObject
{
    static const unsigned ENCLOSING_EXTRA = 0;

  public:
    static DebugScopeObject *create(JSContext *cx, ScopeObject &scope, JSObject &enclosing);

    ScopeObject &scope() const;
    JSObject &enclosingScope() const;

    /* Currently, the 'declarative' scopes are Call and Block. */
    bool isForDeclarative() const;
};

/* Maintains runtime-wide debug scope bookkeeping information. */
class DebugScopes
{
    JSRuntime *rt;

    /* The map from (non-debug) scopes to debug scopes. */
    typedef WeakMap<HeapPtrObject, HeapPtrObject> ObjectWeakMap;
    ObjectWeakMap proxiedScopes;

    /*
     * The map from live frames which have optimized-away scopes to the
     * corresponding debug scopes.
     */
    typedef HashMap<ScopeIter,
                    ReadBarriered<DebugScopeObject>,
                    ScopeIter,
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
                    StackFrame *,
                    DefaultHasher<ScopeObject *>,
                    RuntimeAllocPolicy> LiveScopeMap;
    LiveScopeMap liveScopes;

  public:
    DebugScopes(JSRuntime *rt);
    ~DebugScopes();
    bool init();

    void mark(JSTracer *trc);
    void sweep();

    DebugScopeObject *hasDebugScope(JSContext *cx, ScopeObject &scope) const;
    bool addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope);

    DebugScopeObject *hasDebugScope(JSContext *cx, ScopeIter si) const;
    bool addDebugScope(JSContext *cx, ScopeIter si, DebugScopeObject &debugScope);

    bool updateLiveScopes(JSContext *cx);
    StackFrame *hasLiveFrame(ScopeObject &scope);

    /*
     * In debug-mode, these must be called whenever exiting a call/block or
     * when activating/yielding a generator.
     */
    void onPopCall(StackFrame *fp);
    void onPopBlock(JSContext *cx, StackFrame *fp);
    void onPopWith(StackFrame *fp);
    void onPopStrictEvalScope(StackFrame *fp);
    void onGeneratorFrameChange(StackFrame *from, StackFrame *to);
    void onCompartmentLeaveDebugMode(JSCompartment *c);
};

}  /* namespace js */
#endif /* ScopeObject_h___ */
