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
#include "vm/ProxyObject.h"

namespace js {

namespace frontend { struct Definition; }

class StaticWithObject;

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
template <AllowGC allowGC>
class StaticScopeIter
{
    typename MaybeRooted<JSObject*, allowGC>::RootType obj;
    bool onNamedLambda;

  public:
    StaticScopeIter(ExclusiveContext *cx, JSObject *obj)
      : obj(cx, obj), onNamedLambda(false)
    {
        JS_STATIC_ASSERT(allowGC == CanGC);
        JS_ASSERT_IF(obj, obj->is<StaticBlockObject>() || obj->is<StaticWithObject>() ||
                     obj->is<JSFunction>());
    }

    StaticScopeIter(JSObject *obj)
      : obj((ExclusiveContext *) nullptr, obj), onNamedLambda(false)
    {
        JS_STATIC_ASSERT(allowGC == NoGC);
        JS_ASSERT_IF(obj, obj->is<StaticBlockObject>() || obj->is<StaticWithObject>() ||
                     obj->is<JSFunction>());
    }

    bool done() const;
    void operator++(int);

    /* Return whether this static scope will be on the dynamic scope chain. */
    bool hasDynamicScopeObject() const;
    Shape *scopeShape() const;

    enum Type { WITH, BLOCK, FUNCTION, NAMED_LAMBDA };
    Type type() const;

    StaticBlockObject &block() const;
    StaticWithObject &staticWith() const;
    JSScript *funScript() const;
};

/*****************************************************************************/

/*
 * A "scope coordinate" describes how to get from head of the scope chain to a
 * given lexically-enclosing variable. A scope coordinate has two dimensions:
 *  - hops: the number of scope objects on the scope chain to skip
 *  - slot: the slot on the scope object holding the variable's value
 */
class ScopeCoordinate
{
    uint32_t hops_;
    uint32_t slot_;

    /*
     * Technically, hops_/slot_ are SCOPECOORD_(HOPS|SLOT)_BITS wide.  Since
     * ScopeCoordinate is a temporary value, don't bother with a bitfield as
     * this only adds overhead.
     */
    static_assert(SCOPECOORD_HOPS_BITS <= 32, "We have enough bits below");
    static_assert(SCOPECOORD_SLOT_BITS <= 32, "We have enough bits below");

  public:
    inline ScopeCoordinate(jsbytecode *pc)
      : hops_(GET_SCOPECOORD_HOPS(pc)), slot_(GET_SCOPECOORD_SLOT(pc + SCOPECOORD_HOPS_LEN))
    {
        JS_ASSERT(JOF_OPTYPE(*pc) == JOF_SCOPECOORD);
    }

    inline ScopeCoordinate() {}

    void setHops(uint32_t hops) { JS_ASSERT(hops < SCOPECOORD_HOPS_LIMIT); hops_ = hops; }
    void setSlot(uint32_t slot) { JS_ASSERT(slot < SCOPECOORD_SLOT_LIMIT); slot_ = slot; }

    uint32_t hops() const { JS_ASSERT(hops_ < SCOPECOORD_HOPS_LIMIT); return hops_; }
    uint32_t slot() const { JS_ASSERT(slot_ < SCOPECOORD_SLOT_LIMIT); return slot_; }
};

/*
 * Return a shape representing the static scope containing the variable
 * accessed by the ALIASEDVAR op at 'pc'.
 */
extern Shape *
ScopeCoordinateToStaticScopeShape(JSScript *script, jsbytecode *pc);

/* Return the name being accessed by the given ALIASEDVAR op. */
extern PropertyName *
ScopeCoordinateName(ScopeCoordinateNameCache &cache, JSScript *script, jsbytecode *pc);

/* Return the function script accessed by the given ALIASEDVAR op, or nullptr. */
extern JSScript *
ScopeCoordinateFunctionScript(JSScript *script, jsbytecode *pc);

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
 *     \   \  \
 *      \   \ StaticWithObject  Template for "with" object in static scope chain
 *       \   \
 *        \  DynamicWithObject  Run-time "with" object on scope chain
 *         \
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
        return getFixedSlot(SCOPE_CHAIN_SLOT).toObject();
    }

    void setEnclosingScope(HandleObject obj);

    /*
     * Get or set an aliased variable contained in this scope. Unaliased
     * variables should instead access the StackFrame. Aliased variable access
     * is primarily made through JOF_SCOPECOORD ops which is why these members
     * take a ScopeCoordinate instead of just the slot index.
     */
    inline const Value &aliasedVar(ScopeCoordinate sc);

    inline void setAliasedVar(JSContext *cx, ScopeCoordinate sc, PropertyName *name, const Value &v);

    /* For jit access. */
    static size_t offsetOfEnclosingScope() {
        return getFixedSlotOffset(SCOPE_CHAIN_SLOT);
    }

    static size_t enclosingScopeSlot() {
        return SCOPE_CHAIN_SLOT;
    }
};

class CallObject : public ScopeObject
{
    static const uint32_t CALLEE_SLOT = 1;

    static CallObject *
    create(JSContext *cx, HandleScript script, HandleObject enclosing, HandleFunction callee);

  public:
    static const Class class_;

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
    bool isForEval() const {
        JS_ASSERT(getFixedSlot(CALLEE_SLOT).isObjectOrNull());
        JS_ASSERT_IF(getFixedSlot(CALLEE_SLOT).isObject(),
                     getFixedSlot(CALLEE_SLOT).toObject().is<JSFunction>());
        return getFixedSlot(CALLEE_SLOT).isNull();
    }

    /*
     * Returns the function for which this CallObject was created. (This may
     * only be called if !isForEval.)
     */
    JSFunction &callee() const {
        return getFixedSlot(CALLEE_SLOT).toObject().as<JSFunction>();
    }

    /* Get/set the aliased variable referred to by 'bi'. */
    const Value &aliasedVar(AliasedFormalIter fi) {
        return getSlot(fi.scopeSlot());
    }

    inline void setAliasedVar(JSContext *cx, AliasedFormalIter fi, PropertyName *name, const Value &v);

    /* For jit access. */
    static size_t offsetOfCallee() {
        return getFixedSlotOffset(CALLEE_SLOT);
    }

    static size_t calleeSlot() {
        return CALLEE_SLOT;
    }
};

class DeclEnvObject : public ScopeObject
{
    // Pre-allocated slot for the named lambda.
    static const uint32_t LAMBDA_SLOT = 1;

  public:
    static const uint32_t RESERVED_SLOTS = 2;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT2_BACKGROUND;

    static const Class class_;

    static DeclEnvObject *
    createTemplateObject(JSContext *cx, HandleFunction fun, gc::InitialHeap heap);

    static DeclEnvObject *create(JSContext *cx, HandleObject enclosing, HandleFunction callee);

    static inline size_t lambdaSlot() {
        return LAMBDA_SLOT;
    }
};

class NestedScopeObject : public ScopeObject
{
  public:
    /*
     * A refinement of enclosingScope that returns nullptr if the enclosing
     * scope is not a NestedScopeObject.
     */
    inline NestedScopeObject *enclosingNestedScope() const;

    // Return true if this object is a compile-time scope template.
    inline bool isStatic() { return !getProto(); }

    // Return the static scope corresponding to this scope chain object.
    inline NestedScopeObject* staticScope() {
        JS_ASSERT(!isStatic());
        return &getProto()->as<NestedScopeObject>();
    }

    // At compile-time it's possible for the scope chain to be null.
    JSObject *enclosingScopeForStaticScopeIter() {
        return getReservedSlot(SCOPE_CHAIN_SLOT).toObjectOrNull();
    }

    void initEnclosingNestedScope(JSObject *obj) {
        JS_ASSERT(getReservedSlot(SCOPE_CHAIN_SLOT).isUndefined());
        setReservedSlot(SCOPE_CHAIN_SLOT, ObjectOrNullValue(obj));
    }

    /*
     * The parser uses 'enclosingNestedScope' as the prev-link in the
     * pc->staticScope stack. Note: in the case of hoisting, this prev-link will
     * not ultimately be the same as enclosingNestedScope;
     * initEnclosingNestedScope must be called separately in the
     * emitter. 'reset' is just for asserting stackiness.
     */
    void initEnclosingNestedScopeFromParser(NestedScopeObject *prev) {
        setReservedSlot(SCOPE_CHAIN_SLOT, ObjectOrNullValue(prev));
    }

    void resetEnclosingNestedScopeFromParser() {
        setReservedSlot(SCOPE_CHAIN_SLOT, UndefinedValue());
    }
};

// With scope template objects on the static scope chain.
class StaticWithObject : public NestedScopeObject
{
  public:
    static const unsigned RESERVED_SLOTS = 1;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT2_BACKGROUND;

    static const Class class_;

    static StaticWithObject *create(ExclusiveContext *cx);
};

// With scope objects on the run-time scope chain.
class DynamicWithObject : public NestedScopeObject
{
    static const unsigned OBJECT_SLOT = 1;
    static const unsigned THIS_SLOT = 2;

  public:
    static const unsigned RESERVED_SLOTS = 3;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;

    static const Class class_;

    static DynamicWithObject *
    create(JSContext *cx, HandleObject object, HandleObject enclosing, HandleObject staticWith);

    StaticWithObject& staticWith() const {
        return getProto()->as<StaticWithObject>();
    }

    /* Return the 'o' in 'with (o)'. */
    JSObject &object() const {
        return getReservedSlot(OBJECT_SLOT).toObject();
    }

    /* Return object for the 'this' class hook. */
    JSObject &withThis() const {
        return getReservedSlot(THIS_SLOT).toObject();
    }
};

class BlockObject : public NestedScopeObject
{
  protected:
    static const unsigned DEPTH_SLOT = 1;

  public:
    static const unsigned RESERVED_SLOTS = 2;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;

    static const Class class_;

    /* Return the abstract stack depth right before entering this nested scope. */
    uint32_t stackDepth() const {
        return getReservedSlot(DEPTH_SLOT).toPrivateUint32();
    }

    /* Return the number of variables associated with this block. */
    uint32_t slotCount() const {
        return propertyCount();
    }

  protected:
    /* Blocks contain an object slot for each slot i: 0 <= i < slotCount. */
    const Value &slotValue(unsigned i) {
        return getSlotRef(RESERVED_SLOTS + i);
    }

    void setSlotValue(unsigned i, const Value &v) {
        setSlot(RESERVED_SLOTS + i, v);
    }
};

class StaticBlockObject : public BlockObject
{
    static const unsigned LOCAL_OFFSET_SLOT = 1;

  public:
    static StaticBlockObject *create(ExclusiveContext *cx);

    /* See StaticScopeIter comment. */
    JSObject *enclosingStaticScope() const {
        return getFixedSlot(SCOPE_CHAIN_SLOT).toObjectOrNull();
    }

    /*
     * Return the index (in the range [0, slotCount()) corresponding to the
     * given shape of a block object.
     */
    uint32_t shapeToIndex(const Shape &shape) {
        uint32_t slot = shape.slot();
        JS_ASSERT(slot - RESERVED_SLOTS < slotCount());
        return slot - RESERVED_SLOTS;
    }

    /*
     * A refinement of enclosingStaticScope that returns nullptr if the enclosing
     * static scope is a JSFunction.
     */
    inline StaticBlockObject *enclosingBlock() const;

    uint32_t localOffset() {
        return getReservedSlot(LOCAL_OFFSET_SLOT).toPrivateUint32();
    }

    // Return the local corresponding to the 'var'th binding where 'var' is in the
    // range [0, slotCount()).
    uint32_t varToLocalIndex(uint32_t var) {
        JS_ASSERT(var < slotCount());
        return getReservedSlot(LOCAL_OFFSET_SLOT).toPrivateUint32() + var;
    }

    // Return the slot corresponding to local variable 'local', where 'local' is
    // in the range [localOffset(), localOffset() + slotCount()).  The result is
    // in the range [RESERVED_SLOTS, RESERVED_SLOTS + slotCount()).
    uint32_t localIndexToSlot(uint32_t local) {
        JS_ASSERT(local >= localOffset());
        local -= localOffset();
        JS_ASSERT(local < slotCount());
        return RESERVED_SLOTS + local;
    }

    /*
     * A let binding is aliased if accessed lexically by nested functions or
     * dynamically through dynamic name lookup (eval, with, function::, etc).
     */
    bool isAliased(unsigned i) {
        return slotValue(i).isTrue();
    }

    /*
     * A static block object is cloned (when entering the block) iff some
     * variable of the block isAliased.
     */
    bool needsClone() {
        return !getFixedSlot(RESERVED_SLOTS).isFalse();
    }

    /* Frontend-only functions ***********************************************/

    /* Initialization functions for above fields. */
    void setAliased(unsigned i, bool aliased) {
        JS_ASSERT_IF(i > 0, slotValue(i-1).isBoolean());
        setSlotValue(i, BooleanValue(aliased));
        if (aliased && !needsClone()) {
            setSlotValue(0, MagicValue(JS_BLOCK_NEEDS_CLONE));
            JS_ASSERT(needsClone());
        }
    }

    void setLocalOffset(uint32_t offset) {
        JS_ASSERT(getReservedSlot(LOCAL_OFFSET_SLOT).isUndefined());
        initReservedSlot(LOCAL_OFFSET_SLOT, PrivateUint32Value(offset));
    }

    /*
     * Frontend compilation temporarily uses the object's slots to link
     * a let var to its associated Definition parse node.
     */
    void setDefinitionParseNode(unsigned i, frontend::Definition *def) {
        JS_ASSERT(slotValue(i).isUndefined());
        setSlotValue(i, PrivateValue(def));
    }

    frontend::Definition *definitionParseNode(unsigned i) {
        Value v = slotValue(i);
        return reinterpret_cast<frontend::Definition *>(v.toPrivate());
    }

    /*
     * While ScopeCoordinate can generally reference up to 2^24 slots, block objects have an
     * additional limitation that all slot indices must be storable as uint16_t short-ids in the
     * associated Shape. If we could remove the block dependencies on shape->shortid, we could
     * remove INDEX_LIMIT.
     */
    static const unsigned LOCAL_INDEX_LIMIT = JS_BIT(16);

    static Shape *addVar(ExclusiveContext *cx, Handle<StaticBlockObject*> block, HandleId id,
                         unsigned index, bool *redeclared);
};

class ClonedBlockObject : public BlockObject
{
  public:
    static ClonedBlockObject *create(JSContext *cx, Handle<StaticBlockObject *> block,
                                     AbstractFramePtr frame);

    /* The static block from which this block was cloned. */
    StaticBlockObject &staticBlock() const {
        return getProto()->as<StaticBlockObject>();
    }

    /* Assuming 'put' has been called, return the value of the ith let var. */
    const Value &var(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) {
        JS_ASSERT_IF(checkAliasing, staticBlock().isAliased(i));
        return slotValue(i);
    }

    void setVar(unsigned i, const Value &v, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) {
        JS_ASSERT_IF(checkAliasing, staticBlock().isAliased(i));
        setSlotValue(i, v);
    }

    /* Copy in all the unaliased formals and locals. */
    void copyUnaliasedValues(AbstractFramePtr frame);
};

template<XDRMode mode>
bool
XDRStaticBlockObject(XDRState<mode> *xdr, HandleObject enclosingScope,
                     StaticBlockObject **objp);

template<XDRMode mode>
bool
XDRStaticWithObject(XDRState<mode> *xdr, HandleObject enclosingScope,
                    StaticWithObject **objp);

extern JSObject *
CloneNestedScopeObject(JSContext *cx, HandleObject enclosingScope, Handle<NestedScopeObject*> src);

/*****************************************************************************/

class ScopeIterKey;
class ScopeIterVal;

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
    friend class ScopeIterVal;

  public:
    enum Type { Call, Block, With, StrictEvalScope };

  private:
    JSContext *cx;
    AbstractFramePtr frame_;
    RootedObject cur_;
    Rooted<NestedScopeObject *> staticScope_;
    Type type_;
    bool hasScopeObject_;

    void settle();

    /* ScopeIter does not have value semantics. */
    ScopeIter(const ScopeIter &si) MOZ_DELETE;

    ScopeIter(JSContext *cx) MOZ_DELETE;

  public:

    /* Constructing from a copy of an existing ScopeIter. */
    ScopeIter(const ScopeIter &si, JSContext *cx
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /* Constructing from StackFrame places ScopeIter on the innermost scope. */
    ScopeIter(AbstractFramePtr frame, jsbytecode *pc, JSContext *cx
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /*
     * Without a StackFrame, the resulting ScopeIter is done() with
     * enclosingScope() as given.
     */
    ScopeIter(JSObject &enclosingScope, JSContext *cx
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    ScopeIter(const ScopeIterVal &hashVal, JSContext *cx
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
    NestedScopeObject* staticScope() const { return staticScope_; }

    StaticBlockObject &staticBlock() const {
        JS_ASSERT(type() == Block);
        return staticScope_->as<StaticBlockObject>();
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class ScopeIterKey
{
    friend class ScopeIterVal;

    AbstractFramePtr frame_;
    JSObject *cur_;
    NestedScopeObject *staticScope_;
    ScopeIter::Type type_;
    bool hasScopeObject_;

  public:
    ScopeIterKey(const ScopeIter &si)
      : frame_(si.frame()), cur_(si.cur_), staticScope_(si.staticScope_), type_(si.type_),
        hasScopeObject_(si.hasScopeObject_) {}

    AbstractFramePtr frame() const { return frame_; }
    JSObject *cur() const { return cur_; }
    NestedScopeObject *staticScope() const { return staticScope_; }
    ScopeIter::Type type() const { return type_; }
    bool hasScopeObject() const { return hasScopeObject_; }
    JSObject *enclosingScope() const { return cur_; }
    JSObject *&enclosingScope() { return cur_; }

    /* For use as hash policy */
    typedef ScopeIterKey Lookup;
    static HashNumber hash(ScopeIterKey si);
    static bool match(ScopeIterKey si1, ScopeIterKey si2);
    bool operator!=(const ScopeIterKey &other) const {
        return frame_ != other.frame_ ||
               cur_ != other.cur_ ||
               staticScope_ != other.staticScope_ ||
               type_ != other.type_;
    }
    static void rekey(ScopeIterKey &k, const ScopeIterKey& newKey) {
        k = newKey;
    }
};

class ScopeIterVal
{
    friend class ScopeIter;
    friend class DebugScopes;

    AbstractFramePtr frame_;
    RelocatablePtr<JSObject> cur_;
    RelocatablePtr<NestedScopeObject> staticScope_;
    ScopeIter::Type type_;
    bool hasScopeObject_;

    static void staticAsserts();

  public:
    ScopeIterVal(const ScopeIter &si)
      : frame_(si.frame()), cur_(si.cur_), staticScope_(si.staticScope_), type_(si.type_),
        hasScopeObject_(si.hasScopeObject_) {}

    AbstractFramePtr frame() const { return frame_; }
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
GetDebugScopeForFrame(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc);

/* Provides debugger access to a scope. */
class DebugScopeObject : public ProxyObject
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
    static MOZ_ALWAYS_INLINE void proxiedScopesPostWriteBarrier(JSRuntime *rt, ObjectWeakMap *map,
                                                               const EncapsulatedPtrObject &key);

    /*
     * The map from live frames which have optimized-away scopes to the
     * corresponding debug scopes.
     */
    typedef HashMap<ScopeIterKey,
                    ReadBarriered<DebugScopeObject>,
                    ScopeIterKey,
                    RuntimeAllocPolicy> MissingScopeMap;
    MissingScopeMap missingScopes;
    class MissingScopesRef;
    static MOZ_ALWAYS_INLINE void missingScopesPostWriteBarrier(JSRuntime *rt, MissingScopeMap *map,
                                                               const ScopeIterKey &key);

    /*
     * The map from scope objects of live frames to the live frame. This map
     * updated lazily whenever the debugger needs the information. In between
     * two lazy updates, liveScopes becomes incomplete (but not invalid, onPop*
     * removes scopes as they are popped). Thus, two consecutive debugger lazy
     * updates of liveScopes need only fill in the new scopes.
     */
    typedef HashMap<ScopeObject *,
                    ScopeIterVal,
                    DefaultHasher<ScopeObject *>,
                    RuntimeAllocPolicy> LiveScopeMap;
    LiveScopeMap liveScopes;
    static MOZ_ALWAYS_INLINE void liveScopesPostWriteBarrier(JSRuntime *rt, LiveScopeMap *map,
                                                            ScopeObject *key);

  public:
    DebugScopes(JSContext *c);
    ~DebugScopes();

  private:
    bool init();

    static DebugScopes *ensureCompartmentData(JSContext *cx);

  public:
    void mark(JSTracer *trc);
    void sweep(JSRuntime *rt);
#if defined(JSGC_GENERATIONAL) && defined(JS_GC_ZEAL)
    void checkHashTablesAfterMovingGC(JSRuntime *rt);
#endif

    static DebugScopeObject *hasDebugScope(JSContext *cx, ScopeObject &scope);
    static bool addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope);

    static DebugScopeObject *hasDebugScope(JSContext *cx, const ScopeIter &si);
    static bool addDebugScope(JSContext *cx, const ScopeIter &si, DebugScopeObject &debugScope);

    static bool updateLiveScopes(JSContext *cx);
    static ScopeIterVal *hasLiveScope(ScopeObject &scope);

    // In debug-mode, these must be called whenever exiting a scope that might
    // have stack-allocated locals.
    static void onPopCall(AbstractFramePtr frame, JSContext *cx);
    static void onPopBlock(JSContext *cx, const ScopeIter &si);
    static void onPopBlock(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc);
    static void onPopWith(AbstractFramePtr frame);
    static void onPopStrictEvalScope(AbstractFramePtr frame);
    static void onCompartmentLeaveDebugMode(JSCompartment *c);
};

}  /* namespace js */

template<>
inline bool
JSObject::is<js::NestedScopeObject>() const
{
    return is<js::BlockObject>() || is<js::StaticWithObject>() || is<js::DynamicWithObject>();
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
    extern bool js_IsDebugScopeSlow(js::ProxyObject *proxy);

    // Note: don't use is<ProxyObject>() here -- it also matches subclasses!
    return hasClass(&js::ProxyObject::uncallableClass_) &&
           js_IsDebugScopeSlow(&const_cast<JSObject*>(this)->as<js::ProxyObject>());
}

template<>
inline bool
JSObject::is<js::ClonedBlockObject>() const
{
    return is<js::BlockObject>() && !!getProto();
}

template<>
inline bool
JSObject::is<js::StaticBlockObject>() const
{
    return is<js::BlockObject>() && !getProto();
}

inline JSObject *
JSObject::enclosingScope()
{
    return is<js::ScopeObject>()
           ? &as<js::ScopeObject>().enclosingScope()
           : is<js::DebugScopeObject>()
           ? &as<js::DebugScopeObject>().enclosingScope()
           : getParent();
}

namespace js {

inline const Value &
ScopeObject::aliasedVar(ScopeCoordinate sc)
{
    JS_ASSERT(is<CallObject>() || is<ClonedBlockObject>());
    return getSlot(sc.slot());
}

inline NestedScopeObject *
NestedScopeObject::enclosingNestedScope() const
{
    JSObject *obj = getReservedSlot(SCOPE_CHAIN_SLOT).toObjectOrNull();
    return obj && obj->is<NestedScopeObject>() ? &obj->as<NestedScopeObject>() : nullptr;
}

#ifdef DEBUG
bool
AnalyzeEntrainedVariables(JSContext *cx, HandleScript script);
#endif

} // namespace js

#endif /* vm_ScopeObject_h */
