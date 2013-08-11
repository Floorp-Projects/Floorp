/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonCaches_h
#define jit_IonCaches_h

#ifdef JS_CPU_ARM
# include "jit/arm/Assembler-arm.h"
#endif
#include "jit/IonCode.h"
#include "jit/Registers.h"
#include "jit/shared/Assembler-shared.h"
#include "vm/ForkJoin.h"

class JSFunction;
class JSScript;

namespace js {

class TypedArrayObject;

namespace ion {

#define IONCACHE_KIND_LIST(_)                                   \
    _(GetProperty)                                              \
    _(SetProperty)                                              \
    _(GetElement)                                               \
    _(SetElement)                                               \
    _(BindName)                                                 \
    _(Name)                                                     \
    _(CallsiteClone)                                            \
    _(GetPropertyPar)                                           \
    _(GetElementPar)

// Forward declarations of Cache kinds.
#define FORWARD_DECLARE(kind) class kind##IC;
IONCACHE_KIND_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class IonCacheVisitor
{
  public:
#define VISIT_INS(op)                                               \
    virtual bool visit##op##IC(CodeGenerator *codegen, op##IC *) {  \
        MOZ_ASSUME_UNREACHABLE("NYI: " #op "IC");                   \
    }

    IONCACHE_KIND_LIST(VISIT_INS)
#undef VISIT_INS
};

// Common shared temporary state needed during codegen between the different
// kinds of caches. Used by OutOfLineUpdateCache.
struct AddCacheState
{
    RepatchLabel repatchEntry;
    Register dispatchScratch;
};


// Common structure encoding the state of a polymorphic inline cache contained
// in the code for an IonScript. IonCaches are used for polymorphic operations
// where multiple implementations may be required.
//
// Roughly speaking, the cache initially jumps to an out of line fragment
// which invokes a cache function to perform the operation. The cache function
// may generate a stub to perform the operation in certain cases (e.g. a
// particular shape for an input object) and attach the stub to existing
// stubs, forming a daisy chain of tests for how to perform the operation in
// different circumstances. The details of how stubs are linked up as
// described in comments below for the classes RepatchIonCache and
// DispatchIonCache.
//
// Eventually, if too many stubs are generated the cache function may disable
// the cache, by generating a stub to make a call and perform the operation
// within the VM.
//
// While calls may be made to the cache function and other VM functions, the
// cache may still be treated as pure during optimization passes, such that
// LICM and GVN may be performed on operations around the cache as if the
// operation cannot reenter scripted code through an Invoke() or otherwise have
// unexpected behavior. This restricts the sorts of stubs which the cache can
// generate or the behaviors which called functions can have, and if a called
// function performs a possibly impure operation then the operation will be
// marked as such and the calling script will be recompiled.
//
// Similarly, despite the presence of functions and multiple stubs generated
// for a cache, the cache itself may be marked as idempotent and become hoisted
// or coalesced by LICM or GVN. This also constrains the stubs which can be
// generated for the cache.
//
// * IonCache usage
//
// IonCache is the base structure of an inline cache, which generates code stubs
// dynamically and attaches them to an IonScript.
//
// A cache must at least provide a static update function which will usualy have
// a JSContext*, followed by the cache index. The rest of the arguments of the
// update function are usualy corresponding to the register inputs of the cache,
// as it must perform the same operation as any of the stubs that it might
// produce. The update function call is handled by the visit function of
// CodeGenerator corresponding to this IC.
//
// The CodeGenerator visit function, as opposed to other visit functions, has
// two arguments. The first one is the OutOfLineUpdateCache which stores the LIR
// instruction. The second one is the IC object.  This function would be called
// once the IC is registered with the addCache function of CodeGeneratorShared.
//
// To register a cache, you must call the addCache function as follow:
//
//     MyCodeIC cache(inputReg1, inputValueReg2, outputReg);
//     if (!addCache(lir, allocateCache(cache)))
//         return false;
//
// Once the cache is allocated with the allocateCache function, any modification
// made to the cache would be ignored.
//
// The addCache function will produce a patchable jump at the location where
// it is called. This jump will execute generated stubs and fallback on the code
// of the visitMyCodeIC function if no stub match.
//
//   Warning: As the addCache function fallback on a VMCall, calls to
// addCache should not be in the same path as another VMCall or in the same
// path of another addCache as this is not supported by the invalidation
// procedure.
class IonCache
{
  public:
    class StubAttacher;

    enum Kind {
#   define DEFINE_CACHEKINDS(ickind) Cache_##ickind,
        IONCACHE_KIND_LIST(DEFINE_CACHEKINDS)
#   undef DEFINE_CACHEKINDS
        Cache_Invalid
    };

    // Cache testing and cast.
#   define CACHEKIND_CASTS(ickind)                                      \
    bool is##ickind() const {                                           \
        return kind() == Cache_##ickind;                                \
    }                                                                   \
    inline ickind##IC &to##ickind();                                    \
    inline const ickind##IC &to##ickind() const;
    IONCACHE_KIND_LIST(CACHEKIND_CASTS)
#   undef CACHEKIND_CASTS

    virtual Kind kind() const = 0;

    virtual bool accept(CodeGenerator *codegen, IonCacheVisitor *visitor) = 0;

  public:

    static const char *CacheName(Kind kind);

  protected:
    bool pure_ : 1;
    bool idempotent_ : 1;
    bool disabled_ : 1;
    size_t stubCount_ : 5;

    CodeLocationLabel fallbackLabel_;

    // Location of this operation, NULL for idempotent caches.
    JSScript *script;
    jsbytecode *pc;

  private:
    static const size_t MAX_STUBS;
    void incrementStubCount() {
        // The IC should stop generating stubs before wrapping stubCount.
        stubCount_++;
        JS_ASSERT(stubCount_);
    }

  public:

    IonCache()
      : pure_(false),
        idempotent_(false),
        disabled_(false),
        stubCount_(0),
        fallbackLabel_(),
        script(NULL),
        pc(NULL)
    {
    }

    virtual void disable();
    inline bool isDisabled() const {
        return disabled_;
    }

    // Set the initial 'out-of-line' jump state of the cache. The fallbackLabel is
    // the location of the out-of-line update (slow) path.  This location will
    // be set to the exitJump of the last generated stub.
    void setFallbackLabel(CodeOffsetLabel fallbackLabel) {
        fallbackLabel_ = fallbackLabel;
    }

    virtual void emitInitialJump(MacroAssembler &masm, AddCacheState &addState) = 0;
    virtual void bindInitialJump(MacroAssembler &masm, AddCacheState &addState) = 0;
    virtual void updateBaseAddress(IonCode *code, MacroAssembler &masm);

    // Initialize the AddCacheState depending on the kind of cache, like
    // setting a scratch register. Defaults to doing nothing.
    virtual void initializeAddCacheState(LInstruction *ins, AddCacheState *addState);

    // Reset the cache around garbage collection.
    virtual void reset();

    // Destroy any extra resources the cache uses upon IonScript finalization.
    virtual void destroy();

    bool canAttachStub() const {
        return stubCount_ < MAX_STUBS;
    }
    bool empty() const {
        return stubCount_ == 0;
    }

    enum LinkStatus {
        LINK_ERROR,
        CACHE_FLUSHED,
        LINK_GOOD
    };

    // Use the Linker to link the generated code and check if any
    // monitoring/allocation caused an invalidation of the running ion script,
    // this function returns CACHE_FLUSHED. In case of allocation issue this
    // function returns LINK_ERROR.
    LinkStatus linkCode(JSContext *cx, MacroAssembler &masm, IonScript *ion, IonCode **code);
    // Fixup variables and update jumps in the list of stubs.  Increment the
    // number of attached stubs accordingly.
    void attachStub(MacroAssembler &masm, StubAttacher &attacher, Handle<IonCode *> code);

    // Combine both linkStub and attachStub into one function. In addition, it
    // produces a spew augmented with the attachKind string.
    bool linkAndAttachStub(JSContext *cx, MacroAssembler &masm, StubAttacher &attacher,
                           IonScript *ion, const char *attachKind);

#ifdef DEBUG
    bool isAllocated() {
        return fallbackLabel_.isSet();
    }
#endif

    bool pure() const {
        return pure_;
    }
    bool idempotent() const {
        return idempotent_;
    }
    void setIdempotent() {
        JS_ASSERT(!idempotent_);
        JS_ASSERT(!script);
        JS_ASSERT(!pc);
        idempotent_ = true;
    }

    void setScriptedLocation(JSScript *script, jsbytecode *pc) {
        JS_ASSERT(!idempotent_);
        this->script = script;
        this->pc = pc;
    }

    void getScriptedLocation(MutableHandleScript pscript, jsbytecode **ppc) const {
        pscript.set(script);
        *ppc = pc;
    }
};

//
// Repatch caches initially generate a patchable jump to an out of line call
// to the cache function. Stubs are attached by appending: when attaching a
// new stub, we patch the any failure conditions in last generated stub to
// jump to the new stub. Failure conditions in the new stub jump to the cache
// function which may generate new stubs.
//
//        Control flow               Pointers
//      =======#                 ----.     .---->
//             #                     |     |
//             #======>              \-----/
//
// Initial state:
//
//  JIT Code
// +--------+   .---------------.
// |        |   |               |
// |========|   v +----------+  |
// |== IC ==|====>| Cache Fn |  |
// |========|     +----------+  |
// |        |<=#       #        |
// |        |  #=======#        |
// +--------+  Rejoin path      |
//     |________                |
//             |                |
//   Repatch   |                |
//     IC      |                |
//   Entry     |                |
// +------------+               |
// | lastJump_  |---------------/
// +------------+
// |    ...     |
// +------------+
//
// Attaching stubs:
//
//   Patch the jump pointed to by lastJump_ to jump to the new stub. Update
//   lastJump_ to be the new stub's failure jump. The failure jump of the new
//   stub goes to the fallback label, which is the cache function. In this
//   fashion, new stubs are _appended_ to the chain of stubs, as lastJump_
//   points to the _tail_ of the stub chain.
//
//  JIT Code
// +--------+ #=======================#
// |        | #                       v
// |========| #   +----------+     +------+
// |== IC ==|=#   | Cache Fn |<====| Stub |
// |========|     +----------+  ^  +------+
// |        |<=#      #         |     #
// |        |  #======#=========|=====#
// +--------+      Rejoin path  |
//     |________                |
//             |                |
//   Repatch   |                |
//     IC      |                |
//   Entry     |                |
// +------------+               |
// | lastJump_  |---------------/
// +------------+
// |    ...     |
// +------------+
//
class RepatchIonCache : public IonCache
{
  protected:
    class RepatchStubAppender;

    CodeLocationJump initialJump_;
    CodeLocationJump lastJump_;

    // Offset from the initial jump to the rejoin label.
#ifdef JS_CPU_ARM
    static const size_t REJOIN_LABEL_OFFSET = 4;
#else
    static const size_t REJOIN_LABEL_OFFSET = 0;
#endif

    CodeLocationLabel rejoinLabel() const {
        uint8_t *ptr = initialJump_.raw();
#ifdef JS_CPU_ARM
        uint32_t i = 0;
        while (i < REJOIN_LABEL_OFFSET)
            ptr = Assembler::nextInstruction(ptr, &i);
#endif
        return CodeLocationLabel(ptr);
    }

  public:
    RepatchIonCache()
      : initialJump_(),
        lastJump_()
    {
    }

    virtual void reset();

    // Set the initial jump state of the cache. The initialJump is the inline
    // jump that will point to out-of-line code (such as the slow path, or
    // stubs), and the rejoinLabel is the position that all out-of-line paths
    // will rejoin to.
    void emitInitialJump(MacroAssembler &masm, AddCacheState &addState);
    void bindInitialJump(MacroAssembler &masm, AddCacheState &addState);

    // Update the labels once the code is finalized.
    void updateBaseAddress(IonCode *code, MacroAssembler &masm);
};

//
// Dispatch caches avoid patching already-running code. Instead, the jump to
// the stub chain is indirect by way of the firstStub_ pointer
// below. Initially the pointer points to the cache function which may attach
// new stubs. Stubs are attached by prepending: when attaching a new stub, we
// jump to the previous stub on failure conditions, then overwrite the
// firstStub_ pointer with the newly generated stub.
//
// This style does not patch the already executing instruction stream, does
// not need to worry about cache coherence of cached jump addresses, and does
// not have to worry about aligning the exit jumps to ensure atomic patching,
// at the expense of an extra memory read to load the very first stub.
//
// ICs that need to work in parallel execution need to be dispatch style.
//
//        Control flow               Pointers             Memory load
//      =======#                 ----.     .---->         ******
//             #                     |     |                   *
//             #======>              \-----/                   *******
//
// Initial state:
//
//    The first stub points to the cache function.
//
//  JIT Code
// +--------+                                 .-------.
// |        |                                 v       |
// |========|     +---------------+     +----------+  |
// |== IC ==|====>| Load and jump |====>| Cache Fn |  |
// |========|     +---------------+     +----------+  |
// |        |<=#           *                #         |
// |        |  #===========*================#         |
// +--------+       Rejoin * path                     |
//     |________           *                          |
//             |           *                          |
//   Dispatch  |           *                          |
//     IC    **|************                          |
//   Entry   * |                                      |
// +------------+                                     |
// | firstStub_ |-------------------------------------/
// +------------+
// |    ...     |
// +------------+
//
// Attaching stubs:
//
//   Assign the address of the new stub to firstStub_. The new stub jumps to
//   the old address held in firstStub_ on failure. Note that there is no
//   concept of a fallback label here, new stubs are _prepended_, as
//   firstStub_ always points to the _head_ of the stub chain.
//
//  JIT Code
// +--------+                        #=====================#   .-----.
// |        |                        #                     v   v     |
// |========|     +---------------+  #  +----------+     +------+    |
// |== IC ==|====>| Load and jump |==#  | Cache Fn |<====| Stub |    |
// |========|     +---------------+     +----------+     +------+    |
// |        |<=#           *                #                #       |
// |        |  #===========*================#================#       |
// +--------+       Rejoin * path                                    |
//     |________           *                                         |
//             |           *                                         |
//   Dispatch  |           *                                         |
//     IC    **|************                                         |
//   Entry   * |                                                     |
// +------------+                                                    |
// | firstStub_ |----------------------------------------------------/
// +------------+
// |    ...     |
// +------------+
//
class DispatchIonCache : public IonCache
{
  protected:
    class DispatchStubPrepender;

    uint8_t *firstStub_;
    CodeLocationLabel rejoinLabel_;
    CodeOffsetLabel dispatchLabel_;

  public:
    DispatchIonCache()
      : firstStub_(NULL),
        rejoinLabel_(),
        dispatchLabel_()
    {
    }

    virtual void reset();
    virtual void initializeAddCacheState(LInstruction *ins, AddCacheState *addState);

    void emitInitialJump(MacroAssembler &masm, AddCacheState &addState);
    void bindInitialJump(MacroAssembler &masm, AddCacheState &addState);

    // Fix up the first stub pointer once the code is finalized.
    void updateBaseAddress(IonCode *code, MacroAssembler &masm);
};

// Define the cache kind and pre-declare data structures used for calling inline
// caches.
#define CACHE_HEADER(ickind)                                        \
    Kind kind() const {                                             \
        return IonCache::Cache_##ickind;                            \
    }                                                               \
                                                                    \
    bool accept(CodeGenerator *codegen, IonCacheVisitor *visitor) { \
        return visitor->visit##ickind##IC(codegen, this);           \
    }                                                               \
                                                                    \
    static const VMFunction UpdateInfo;

// Subclasses of IonCache for the various kinds of caches. These do not define
// new data members; all caches must be of the same size.

class GetPropertyIC : public RepatchIonCache
{
  protected:
    // Registers live after the cache, excluding output registers. The initial
    // value of these registers must be preserved by the cache.
    RegisterSet liveRegs_;

    Register object_;
    PropertyName *name_;
    TypedOrValueRegister output_;
    bool allowGetters_ : 1;
    bool hasTypedArrayLengthStub_ : 1;
    bool hasStrictArgumentsLengthStub_ : 1;
    bool hasNormalArgumentsLengthStub_ : 1;
    bool hasGenericProxyStub_ : 1;

  public:
    GetPropertyIC(RegisterSet liveRegs,
                  Register object, PropertyName *name,
                  TypedOrValueRegister output,
                  bool allowGetters)
      : liveRegs_(liveRegs),
        object_(object),
        name_(name),
        output_(output),
        allowGetters_(allowGetters),
        hasTypedArrayLengthStub_(false),
        hasStrictArgumentsLengthStub_(false),
        hasNormalArgumentsLengthStub_(false),
        hasGenericProxyStub_(false)
    {
    }

    CACHE_HEADER(GetProperty)

    void reset();

    Register object() const {
        return object_;
    }
    PropertyName *name() const {
        return name_;
    }
    TypedOrValueRegister output() const {
        return output_;
    }
    bool allowGetters() const {
        return allowGetters_ && !idempotent();
    }
    bool hasTypedArrayLengthStub() const {
        return hasTypedArrayLengthStub_;
    }
    bool hasArgumentsLengthStub(bool strict) const {
        return strict ? hasStrictArgumentsLengthStub_ : hasNormalArgumentsLengthStub_;
    }
    bool hasGenericProxyStub() const {
        return hasGenericProxyStub_;
    }

    enum NativeGetPropCacheability {
        CanAttachError,
        CanAttachNone,
        CanAttachReadSlot,
        CanAttachArrayLength,
        CanAttachCallGetter
    };

    // Helpers for CanAttachNativeGetProp
    typedef JSContext * Context;
    static bool doPropertyLookup(Context cx, HandleObject obj, HandlePropertyName name,
                                 MutableHandleObject holder, MutableHandleShape shape) {
        return JSObject::lookupProperty(cx, obj, name, holder, shape);
    }
    bool lookupNeedsIdempotentChain() const {
        return idempotent();
    }
    bool canMonitorSingletonUndefinedSlot(HandleObject holder, HandleShape shape) const;

    // Attach the proper stub, if possible
    bool tryAttachStub(JSContext *cx, IonScript *ion, HandleObject obj,
                       HandlePropertyName name, void *returnAddr, bool *emitted);
    bool tryAttachProxy(JSContext *cx, IonScript *ion, HandleObject obj,
                        HandlePropertyName name, void *returnAddr, bool *emitted);
    bool tryAttachGenericProxy(JSContext *cx, IonScript *ion, HandleObject obj,
                               HandlePropertyName name, void *returnAddr, bool *emitted);
    bool tryAttachDOMProxyShadowed(JSContext *cx, IonScript *ion, HandleObject obj,
                                   void *returnAddr, bool *emitted);
    bool tryAttachDOMProxyUnshadowed(JSContext *cx, IonScript *ion, HandleObject obj,
                                     HandlePropertyName name, bool resetNeeded,
                                     void *returnAddr, bool *emitted);
    bool tryAttachNative(JSContext *cx, IonScript *ion, HandleObject obj,
                         HandlePropertyName name, void *returnAddr, bool *emitted);
    bool tryAttachTypedArrayLength(JSContext *cx, IonScript *ion, HandleObject obj,
                                   HandlePropertyName name, bool *emitted);

    bool tryAttachArgumentsLength(JSContext *cx, IonScript *ion, HandleObject obj,
                                  HandlePropertyName name, bool *emitted);

    static bool update(JSContext *cx, size_t cacheIndex, HandleObject obj, MutableHandleValue vp);
};

class SetPropertyIC : public RepatchIonCache
{
  protected:
    // Registers live after the cache, excluding output registers. The initial
    // value of these registers must be preserved by the cache.
    RegisterSet liveRegs_;

    Register object_;
    PropertyName *name_;
    ConstantOrRegister value_;
    bool isSetName_;
    bool strict_;

  public:
    SetPropertyIC(RegisterSet liveRegs, Register object, PropertyName *name,
                  ConstantOrRegister value, bool isSetName, bool strict)
      : liveRegs_(liveRegs),
        object_(object),
        name_(name),
        value_(value),
        isSetName_(isSetName),
        strict_(strict)
    {
    }

    CACHE_HEADER(SetProperty)

    Register object() const {
        return object_;
    }
    PropertyName *name() const {
        return name_;
    }
    ConstantOrRegister value() const {
        return value_;
    }
    bool isSetName() const {
        return isSetName_;
    }
    bool strict() const {
        return strict_;
    }

    bool attachNativeExisting(JSContext *cx, IonScript *ion, HandleObject obj, HandleShape shape);
    bool attachSetterCall(JSContext *cx, IonScript *ion, HandleObject obj,
                          HandleObject holder, HandleShape shape, void *returnAddr);
    bool attachNativeAdding(JSContext *cx, IonScript *ion, JSObject *obj, HandleShape oldshape,
                            HandleShape newshape, HandleShape propshape);

    static bool
    update(JSContext *cx, size_t cacheIndex, HandleObject obj, HandleValue value);
};

class GetElementIC : public RepatchIonCache
{
  protected:
    Register object_;
    ConstantOrRegister index_;
    TypedOrValueRegister output_;

    bool monitoredResult_ : 1;
    bool hasDenseStub_ : 1;
    bool hasStrictArgumentsStub_ : 1;
    bool hasNormalArgumentsStub_ : 1;

    size_t failedUpdates_;

    static const size_t MAX_FAILED_UPDATES;

  public:
    GetElementIC(Register object, ConstantOrRegister index,
                 TypedOrValueRegister output, bool monitoredResult)
      : object_(object),
        index_(index),
        output_(output),
        monitoredResult_(monitoredResult),
        hasDenseStub_(false),
        hasStrictArgumentsStub_(false),
        hasNormalArgumentsStub_(false),
        failedUpdates_(0)
    {
    }

    CACHE_HEADER(GetElement)

    void reset();

    Register object() const {
        return object_;
    }
    ConstantOrRegister index() const {
        return index_;
    }
    TypedOrValueRegister output() const {
        return output_;
    }
    bool monitoredResult() const {
        return monitoredResult_;
    }
    bool hasDenseStub() const {
        return hasDenseStub_;
    }
    bool hasArgumentsStub(bool strict) const {
        return strict ? hasStrictArgumentsStub_ : hasNormalArgumentsStub_;
    }
    void setHasDenseStub() {
        JS_ASSERT(!hasDenseStub());
        hasDenseStub_ = true;
    }

    static bool canAttachGetProp(JSObject *obj, const Value &idval, jsid id);
    static bool canAttachDenseElement(JSObject *obj, const Value &idval);
    static bool canAttachTypedArrayElement(JSObject *obj, const Value &idval,
                                           TypedOrValueRegister output);

    bool attachGetProp(JSContext *cx, IonScript *ion, HandleObject obj, const Value &idval, HandlePropertyName name);
    bool attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval);
    bool attachTypedArrayElement(JSContext *cx, IonScript *ion, TypedArrayObject *tarr,
                                 const Value &idval);
    bool attachArgumentsElement(JSContext *cx, IonScript *ion, JSObject *obj);

    static bool
    update(JSContext *cx, size_t cacheIndex, HandleObject obj, HandleValue idval,
                MutableHandleValue vp);

    void incFailedUpdates() {
        failedUpdates_++;
    }
    void resetFailedUpdates() {
        failedUpdates_ = 0;
    }
    bool shouldDisable() const {
        return !canAttachStub() ||
               (stubCount_ == 0 && failedUpdates_ > MAX_FAILED_UPDATES);
    }
};

class SetElementIC : public RepatchIonCache
{
  protected:
    Register object_;
    Register tempToUnboxIndex_;
    Register temp_;
    ValueOperand index_;
    ConstantOrRegister value_;
    bool strict_;

    bool hasDenseStub_ : 1;

  public:
    SetElementIC(Register object, Register tempToUnboxIndex, Register temp,
                 ValueOperand index, ConstantOrRegister value,
                 bool strict)
      : object_(object),
        tempToUnboxIndex_(tempToUnboxIndex),
        temp_(temp),
        index_(index),
        value_(value),
        strict_(strict),
        hasDenseStub_(false)
    {
    }

    CACHE_HEADER(SetElement)

    void reset();

    Register object() const {
        return object_;
    }
    Register tempToUnboxIndex() const {
        return temp_;
    }
    Register temp() const {
        return temp_;
    }
    ValueOperand index() const {
        return index_;
    }
    ConstantOrRegister value() const {
        return value_;
    }
    bool strict() const {
        return strict_;
    }

    bool hasDenseStub() const {
        return hasDenseStub_;
    }
    void setHasDenseStub() {
        JS_ASSERT(!hasDenseStub());
        hasDenseStub_ = true;
    }

    bool attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval);

    static bool
    update(JSContext *cx, size_t cacheIndex, HandleObject obj, HandleValue idval,
                HandleValue value);
};

class BindNameIC : public RepatchIonCache
{
  protected:
    Register scopeChain_;
    PropertyName *name_;
    Register output_;

  public:
    BindNameIC(Register scopeChain, PropertyName *name, Register output)
      : scopeChain_(scopeChain),
        name_(name),
        output_(output)
    {
    }

    CACHE_HEADER(BindName)

    Register scopeChainReg() const {
        return scopeChain_;
    }
    HandlePropertyName name() const {
        return HandlePropertyName::fromMarkedLocation(&name_);
    }
    Register outputReg() const {
        return output_;
    }

    bool attachGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain);
    bool attachNonGlobal(JSContext *cx, IonScript *ion, JSObject *scopeChain, JSObject *holder);

    static JSObject *
    update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain);
};

class NameIC : public RepatchIonCache
{
  protected:
    // Registers live after the cache, excluding output registers. The initial
    // value of these registers must be preserved by the cache.
    RegisterSet liveRegs_;

    bool typeOf_;
    Register scopeChain_;
    PropertyName *name_;
    TypedOrValueRegister output_;

  public:
    NameIC(RegisterSet liveRegs, bool typeOf,
           Register scopeChain, PropertyName *name,
           TypedOrValueRegister output)
      : liveRegs_(liveRegs),
        typeOf_(typeOf),
        scopeChain_(scopeChain),
        name_(name),
        output_(output)
    {
    }

    CACHE_HEADER(Name)

    Register scopeChainReg() const {
        return scopeChain_;
    }
    HandlePropertyName name() const {
        return HandlePropertyName::fromMarkedLocation(&name_);
    }
    TypedOrValueRegister outputReg() const {
        return output_;
    }
    bool isTypeOf() const {
        return typeOf_;
    }

    bool attachReadSlot(JSContext *cx, IonScript *ion, HandleObject scopeChain, HandleObject obj,
                        HandleShape shape);
    bool attachCallGetter(JSContext *cx, IonScript *ion, JSObject *obj, JSObject *holder,
                          HandleShape shape, void *returnAddr);

    static bool
    update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain, MutableHandleValue vp);
};

class CallsiteCloneIC : public RepatchIonCache
{
  protected:
    Register callee_;
    Register output_;
    JSScript *callScript_;
    jsbytecode *callPc_;

  public:
    CallsiteCloneIC(Register callee, JSScript *callScript, jsbytecode *callPc, Register output)
      : callee_(callee),
        output_(output),
        callScript_(callScript),
        callPc_(callPc)
    {
    }

    CACHE_HEADER(CallsiteClone)

    Register calleeReg() const {
        return callee_;
    }
    HandleScript callScript() const {
        return HandleScript::fromMarkedLocation(&callScript_);
    }
    jsbytecode *callPc() const {
        return callPc_;
    }
    Register outputReg() const {
        return output_;
    }

    bool attach(JSContext *cx, IonScript *ion, HandleFunction original, HandleFunction clone);

    static JSObject *update(JSContext *cx, size_t cacheIndex, HandleObject callee);
};

class ParallelIonCache : public DispatchIonCache
{
  protected:
    // A set of all objects that are stubbed. Used to detect duplicates in
    // parallel execution.
    ShapeSet *stubbedShapes_;

    ParallelIonCache()
      : stubbedShapes_(NULL)
    {
    }

    bool initStubbedShapes(JSContext *cx);

  public:
    void reset();
    void destroy();

    bool hasOrAddStubbedShape(LockedJSContext &cx, Shape *shape, bool *alreadyStubbed);
};

class GetPropertyParIC : public ParallelIonCache
{
  protected:
    Register object_;
    PropertyName *name_;
    TypedOrValueRegister output_;
    bool hasTypedArrayLengthStub_ : 1;

   public:
    GetPropertyParIC(Register object, PropertyName *name, TypedOrValueRegister output)
      : object_(object),
        name_(name),
        output_(output),
        hasTypedArrayLengthStub_(false)
    {
    }

    CACHE_HEADER(GetPropertyPar)

#ifdef JS_CPU_X86
    // x86 lacks a general purpose scratch register for dispatch caches and
    // must be given one manually.
    void initializeAddCacheState(LInstruction *ins, AddCacheState *addState);
#endif

    void reset();

    Register object() const {
        return object_;
    }
    PropertyName *name() const {
        return name_;
    }
    TypedOrValueRegister output() const {
        return output_;
    }
    bool hasTypedArrayLengthStub() const {
        return hasTypedArrayLengthStub_;
    }

    static bool canAttachReadSlot(LockedJSContext &cx, IonCache &cache,
                                  TypedOrValueRegister output, JSObject *obj,
                                  PropertyName *name, MutableHandleObject holder,
                                  MutableHandleShape shape);

    bool attachReadSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj, JSObject *holder,
                        Shape *shape);
    bool attachArrayLength(LockedJSContext &cx, IonScript *ion, JSObject *obj);
    bool attachTypedArrayLength(LockedJSContext &cx, IonScript *ion, JSObject *obj);

    static ParallelResult update(ForkJoinSlice *slice, size_t cacheIndex, HandleObject obj,
                                 MutableHandleValue vp);
};

class GetElementParIC : public ParallelIonCache
{
  protected:
    Register object_;
    ConstantOrRegister index_;
    TypedOrValueRegister output_;

    bool monitoredResult_ : 1;

  public:
    GetElementParIC(Register object, ConstantOrRegister index,
                    TypedOrValueRegister output, bool monitoredResult)
      : object_(object),
        index_(index),
        output_(output),
        monitoredResult_(monitoredResult)
    {
    }

    CACHE_HEADER(GetElementPar)

#ifdef JS_CPU_X86
    // x86 lacks a general purpose scratch register for dispatch caches and
    // must be given one manually.
    void initializeAddCacheState(LInstruction *ins, AddCacheState *addState);
#endif

    Register object() const {
        return object_;
    }
    ConstantOrRegister index() const {
        return index_;
    }
    TypedOrValueRegister output() const {
        return output_;
    }
    bool monitoredResult() const {
        return monitoredResult_;
    }

    bool attachReadSlot(LockedJSContext &cx, IonScript *ion, JSObject *obj, const Value &idval,
                        PropertyName *name, JSObject *holder, Shape *shape);
    bool attachDenseElement(LockedJSContext &cx, IonScript *ion, JSObject *obj, const Value &idval);
    bool attachTypedArrayElement(LockedJSContext &cx, IonScript *ion, TypedArrayObject *tarr,
                                 const Value &idval);

    static ParallelResult update(ForkJoinSlice *slice, size_t cacheIndex, HandleObject obj, HandleValue idval,
                                 MutableHandleValue vp);

};

#undef CACHE_HEADER

// Implement cache casts now that the compiler can see the inheritance.
#define CACHE_CASTS(ickind)                                             \
    ickind##IC &IonCache::to##ickind()                                  \
    {                                                                   \
        JS_ASSERT(is##ickind());                                        \
        return *static_cast<ickind##IC *>(this);                        \
    }                                                                   \
    const ickind##IC &IonCache::to##ickind() const                      \
    {                                                                   \
        JS_ASSERT(is##ickind());                                        \
        return *static_cast<const ickind##IC *>(this);                  \
    }
IONCACHE_KIND_LIST(CACHE_CASTS)
#undef OPCODE_CASTS

} // namespace ion
} // namespace js

#endif /* jit_IonCaches_h */
