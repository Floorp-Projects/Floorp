/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_caches_h__
#define jsion_caches_h__

#include "IonCode.h"
#include "TypeOracle.h"
#include "Registers.h"

class JSFunction;
class JSScript;

namespace js {
namespace ion {

#define IONCACHE_KIND_LIST(_)                                   \
    _(GetProperty)                                              \
    _(SetProperty)                                              \
    _(GetElement)                                               \
    _(BindName)                                                 \
    _(Name)                                                     \
    _(CallsiteClone)

// Forward declarations of Cache kinds.
#define FORWARD_DECLARE(kind) class kind##IC;
IONCACHE_KIND_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class IonCacheVisitor
{
  public:
#define VISIT_INS(op)                                               \
    virtual bool visit##op##IC(CodeGenerator *codegen, op##IC *) {  \
        JS_NOT_REACHED("NYI: " #op "IC");                           \
        return false;                                               \
    }

    IONCACHE_KIND_LIST(VISIT_INS)
#undef VISIT_INS
};

// Common structure encoding the state of a polymorphic inline cache contained
// in the code for an IonScript. IonCaches are used for polymorphic operations
// where multiple implementations may be required.
//
// The cache is initially compiled as a patchable jump to an out of line
// fragment which invokes a cache function to perform the operation. The cache
// function may generate a stub to perform the operation in certain cases
// (e.g. a particular shape for an input object), patch the cache's jump to
// that stub and patch any failure conditions in the stub to jump back to the
// cache fragment. When those failure conditions are hit, the cache function
// may attach new stubs, forming a daisy chain of tests for how to perform the
// operation in different circumstances.
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
    inline ickind##IC &to##ickind();

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

    CodeLocationJump initialJump_;
    CodeLocationJump lastJump_;
    CodeLocationLabel fallbackLabel_;

    // Offset from the initial jump to the rejoin label.
#ifdef JS_CPU_ARM
    static const size_t REJOIN_LABEL_OFFSET = 4;
#else
    static const size_t REJOIN_LABEL_OFFSET = 0;
#endif

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

    CodeLocationLabel fallbackLabel() const {
        return fallbackLabel_;
    }
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

    IonCache()
      : pure_(false),
        idempotent_(false),
        disabled_(false),
        stubCount_(0),
        initialJump_(),
        lastJump_(),
        fallbackLabel_(),
        script(NULL),
        pc(NULL)
    {
    }

    void disable();
    inline bool isDisabled() const {
        return disabled_;
    }

    // Set the initial jump state of the cache. The initialJump is the inline
    // jump that will point to out-of-line code (such as the slow path, or
    // stubs), and the rejoinLabel is the position that all out-of-line paths
    // will rejoin to.
    void setInlineJump(CodeOffsetJump initialJump, CodeOffsetLabel rejoinLabel) {
        initialJump_ = initialJump;
        lastJump_ = initialJump;

        JS_ASSERT(rejoinLabel.offset() == initialJump.offset() + REJOIN_LABEL_OFFSET);
    }

    // Set the initial 'out-of-line' jump state of the cache. The fallbackLabel is
    // the location of the out-of-line update (slow) path.  This location will
    // be set to the exitJump of the last generated stub.
    void setFallbackLabel(CodeOffsetLabel fallbackLabel) {
        fallbackLabel_ = fallbackLabel;
    }

    // Update labels once the code is copied and finalized.
    void updateBaseAddress(IonCode *code, MacroAssembler &masm);

    // Reset the cache around garbage collection.
    void reset();

    bool canAttachStub() const {
        return stubCount_ < MAX_STUBS;
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
    void attachStub(MacroAssembler &masm, IonCode *code, CodeOffsetJump &rejoinOffset,
                    CodeOffsetJump *exitOffset, CodeOffsetLabel *stubOffset = NULL);

    // Combine both linkCode and attachStub into one function. In addition, it
    // produces a spew augmented with the attachKind string.
    bool linkAndAttachStub(JSContext *cx, MacroAssembler &masm, IonScript *ion,
                           const char *attachKind, CodeOffsetJump &rejoinOffset,
                           CodeOffsetJump *exitOffset, CodeOffsetLabel *stubOffset = NULL);

    bool pure() {
        return pure_;
    }
    bool idempotent() {
        return idempotent_;
    }
    void setIdempotent() {
        JS_ASSERT(!idempotent_);
        JS_ASSERT(!script);
        JS_ASSERT(!pc);
        idempotent_ = true;
    }

    void setScriptedLocation(RawScript script, jsbytecode *pc) {
        JS_ASSERT(!idempotent_);
        this->script = script;
        this->pc = pc;
    }

    void getScriptedLocation(MutableHandleScript pscript, jsbytecode **ppc) {
        pscript.set(script);
        *ppc = pc;
    }
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

class GetPropertyIC : public IonCache
{
  protected:
    // Registers live after the cache, excluding output registers. The initial
    // value of these registers must be preserved by the cache.
    RegisterSet liveRegs_;

    Register object_;
    PropertyName *name_;
    TypedOrValueRegister output_;
    bool allowGetters_ : 1;
    bool hasArrayLengthStub_ : 1;
    bool hasTypedArrayLengthStub_ : 1;

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
        hasArrayLengthStub_(false),
        hasTypedArrayLengthStub_(false)
    {
    }

    CACHE_HEADER(GetProperty)

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
        return allowGetters_;
    }
    bool hasArrayLengthStub() const {
        return hasArrayLengthStub_;
    }
    bool hasTypedArrayLengthStub() const {
        return hasTypedArrayLengthStub_;
    }

    bool attachReadSlot(JSContext *cx, IonScript *ion, JSObject *obj, JSObject *holder,
                        HandleShape shape);
    bool attachCallGetter(JSContext *cx, IonScript *ion, JSObject *obj, JSObject *holder,
                          HandleShape shape,
                          const SafepointIndex *safepointIndex, void *returnAddr);
    bool attachArrayLength(JSContext *cx, IonScript *ion, JSObject *obj);
    bool attachTypedArrayLength(JSContext *cx, IonScript *ion, JSObject *obj);

    static bool update(JSContext *cx, size_t cacheIndex, HandleObject obj, MutableHandleValue vp);
};

class SetPropertyIC : public IonCache
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

class GetElementIC : public IonCache
{
  protected:
    Register object_;
    ConstantOrRegister index_;
    TypedOrValueRegister output_;

    bool monitoredResult_ : 1;
    bool hasDenseStub_ : 1;

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
        failedUpdates_(0)
    {
    }

    CACHE_HEADER(GetElement)

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
    void setHasDenseStub() {
        JS_ASSERT(!hasDenseStub());
        hasDenseStub_ = true;
    }

    bool attachGetProp(JSContext *cx, IonScript *ion, HandleObject obj, const Value &idval, HandlePropertyName name);
    bool attachDenseElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval);
    bool attachTypedArrayElement(JSContext *cx, IonScript *ion, JSObject *obj, const Value &idval);

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

class BindNameIC : public IonCache
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

class NameIC : public IonCache
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
                          HandleShape shape, const SafepointIndex *safepointIndex,
                          void *returnAddr);

    static bool
    update(JSContext *cx, size_t cacheIndex, HandleObject scopeChain, MutableHandleValue vp);
};

class CallsiteCloneIC : public IonCache
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

#undef CACHE_HEADER

// Implement cache casts now that the compiler can see the inheritance.
#define CACHE_CASTS(ickind)                                             \
    ickind##IC &IonCache::to##ickind()                                  \
    {                                                                   \
        JS_ASSERT(is##ickind());                                        \
        return *static_cast<ickind##IC *>(this);                        \
    }
IONCACHE_KIND_LIST(CACHE_CASTS)
#undef OPCODE_CASTS

} // namespace ion
} // namespace js

#endif // jsion_caches_h__
