/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonIC_h
#define jit_IonIC_h

#include "jit/CacheIR.h"

namespace js {
namespace jit {

class CacheIRStubInfo;

// An optimized stub attached to an IonIC.
class IonICStub
{
    // Code to jump to when this stub fails. This is either the next optimized
    // stub or the OOL fallback path.
    uint8_t* nextCodeRaw_;

    // The next optimized stub in this chain, or nullptr if this is the last
    // one.
    IonICStub* next_;

    // Info about this stub.
    CacheIRStubInfo* stubInfo_;

  public:
    IonICStub(uint8_t* fallbackCode, CacheIRStubInfo* stubInfo)
      : nextCodeRaw_(fallbackCode), next_(nullptr), stubInfo_(stubInfo)
    {}

    uint8_t* nextCodeRaw() const { return nextCodeRaw_; }
    uint8_t** nextCodeRawPtr() { return &nextCodeRaw_; }
    CacheIRStubInfo* stubInfo() const { return stubInfo_; }
    IonICStub* next() const { return next_; }

    uint8_t* stubDataStart();

    void setNext(IonICStub* next, JitCode* nextCode) {
        MOZ_ASSERT(!next_);
        MOZ_ASSERT(next && nextCode);
        next_ = next;
        nextCodeRaw_ = nextCode->raw();
    }

    // Null out pointers when we unlink stubs, to ensure we never use
    // discarded stubs.
    void poison() {
        nextCodeRaw_ = nullptr;
        next_ = nullptr;
        stubInfo_ = nullptr;
    }
};

class IonGetPropertyIC;
class IonSetPropertyIC;
class IonGetNameIC;
class IonBindNameIC;
class IonHasOwnIC;

class IonIC
{
    // This either points at the OOL path for the fallback path, or the code for
    // the first stub.
    uint8_t* codeRaw_;

    // The first optimized stub, or nullptr.
    IonICStub* firstStub_;

    // The address stubs should jump to when done.
    CodeLocationLabel rejoinLabel_;

    // The OOL path that calls the IC's update function.
    CodeLocationLabel fallbackLabel_;

    // Location of this IC, nullptr for idempotent caches.
    JSScript* script_;
    jsbytecode* pc_;

    CacheKind kind_;
    bool idempotent_ : 1;
    ICState state_;

  protected:
    explicit IonIC(CacheKind kind)
      : codeRaw_(nullptr),
        firstStub_(nullptr),
        rejoinLabel_(),
        fallbackLabel_(),
        script_(nullptr),
        pc_(nullptr),
        kind_(kind),
        idempotent_(false),
        state_()
    {}

    void attachStub(IonICStub* newStub, JitCode* code);

  public:
    void setScriptedLocation(JSScript* script, jsbytecode* pc) {
        MOZ_ASSERT(!script_ && !pc_);
        MOZ_ASSERT(script && pc);
        script_ = script;
        pc_ = pc;
    }

    JSScript* script() const { MOZ_ASSERT(script_); return script_; }
    jsbytecode* pc() const { MOZ_ASSERT(pc_); return pc_; }

    CodeLocationLabel rejoinLabel() const { return rejoinLabel_; }

    // Discard all stubs.
    void discardStubs(Zone* zone);

    // Discard all stubs and reset the ICState.
    void reset(Zone* zone);

    ICState& state() {
        return state_;
    }

    CacheKind kind() const { return kind_; }
    uint8_t** codeRawPtr() { return &codeRaw_; }

    bool idempotent() const { return idempotent_; }
    void setIdempotent() { idempotent_ = true; }

    void setFallbackLabel(CodeOffset fallbackLabel) { fallbackLabel_ = fallbackLabel; }
    void setRejoinLabel(CodeOffset rejoinLabel) { rejoinLabel_ = rejoinLabel; }

    IonGetPropertyIC* asGetPropertyIC() {
        MOZ_ASSERT(kind_ == CacheKind::GetProp || kind_ == CacheKind::GetElem);
        return (IonGetPropertyIC*)this;
    }
    IonSetPropertyIC* asSetPropertyIC() {
        MOZ_ASSERT(kind_ == CacheKind::SetProp || kind_ == CacheKind::SetElem);
        return (IonSetPropertyIC*)this;
    }
    IonGetNameIC* asGetNameIC() {
        MOZ_ASSERT(kind_ == CacheKind::GetName);
        return (IonGetNameIC*)this;
    }
    IonBindNameIC* asBindNameIC() {
        MOZ_ASSERT(kind_ == CacheKind::BindName);
        return (IonBindNameIC*)this;
    }
    IonHasOwnIC* asHasOwnIC() {
        MOZ_ASSERT(kind_ == CacheKind::HasOwn);
        return (IonHasOwnIC*)this;
    }

    void updateBaseAddress(JitCode* code, MacroAssembler& masm);

    // Returns the Register to use as scratch when entering IC stubs. This
    // should either be an output register or a temp.
    Register scratchRegisterForEntryJump();

    void trace(JSTracer* trc);

    void attachCacheIRStub(JSContext* cx, const CacheIRWriter& writer, CacheKind kind,
                           IonScript* ionScript, bool* attached,
                           const PropertyTypeCheckInfo* typeCheckInfo = nullptr);
};

class IonGetPropertyIC : public IonIC
{
    LiveRegisterSet liveRegs_;

    TypedOrValueRegister value_;
    ConstantOrRegister id_;
    TypedOrValueRegister output_;
    Register maybeTemp_; // Might be InvalidReg.

    bool monitoredResult_ : 1;
    bool allowDoubleResult_ : 1;

  public:
    IonGetPropertyIC(CacheKind kind, LiveRegisterSet liveRegs, TypedOrValueRegister value,
                     const ConstantOrRegister& id, TypedOrValueRegister output, Register maybeTemp,
                     bool monitoredResult, bool allowDoubleResult)
      : IonIC(kind),
        liveRegs_(liveRegs),
        value_(value),
        id_(id),
        output_(output),
        maybeTemp_(maybeTemp),
        monitoredResult_(monitoredResult),
        allowDoubleResult_(allowDoubleResult)
    { }

    bool monitoredResult() const { return monitoredResult_; }
    TypedOrValueRegister value() const { return value_; }
    ConstantOrRegister id() const { return id_; }
    TypedOrValueRegister output() const { return output_; }
    Register maybeTemp() const { return maybeTemp_; }
    LiveRegisterSet liveRegs() const { return liveRegs_; }
    bool allowDoubleResult() const { return allowDoubleResult_; }

    static MOZ_MUST_USE bool update(JSContext* cx, HandleScript outerScript, IonGetPropertyIC* ic,
                                    HandleValue val, HandleValue idVal, MutableHandleValue res);
};

class IonSetPropertyIC : public IonIC
{
    LiveRegisterSet liveRegs_;

    Register object_;
    Register temp_;
    FloatRegister maybeTempDouble_;
    FloatRegister maybeTempFloat32_;
    ConstantOrRegister id_;
    ConstantOrRegister rhs_;
    bool strict_ : 1;
    bool needsPostBarrier_ : 1;
    bool needsTypeBarrier_ : 1;
    bool guardHoles_ : 1;

  public:
    IonSetPropertyIC(CacheKind kind, LiveRegisterSet liveRegs, Register object, Register temp,
                     FloatRegister maybeTempDouble, FloatRegister maybeTempFloat32,
                     const ConstantOrRegister& id, const ConstantOrRegister& rhs, bool strict,
                     bool needsPostBarrier, bool needsTypeBarrier, bool guardHoles)
      : IonIC(kind),
        liveRegs_(liveRegs),
        object_(object),
        temp_(temp),
        maybeTempDouble_(maybeTempDouble),
        maybeTempFloat32_(maybeTempFloat32),
        id_(id),
        rhs_(rhs),
        strict_(strict),
        needsPostBarrier_(needsPostBarrier),
        needsTypeBarrier_(needsTypeBarrier),
        guardHoles_(guardHoles)
    { }

    LiveRegisterSet liveRegs() const { return liveRegs_; }
    Register object() const { return object_; }
    ConstantOrRegister id() const { return id_; }
    ConstantOrRegister rhs() const { return rhs_; }

    Register temp() const { return temp_; }
    FloatRegister maybeTempDouble() const { return maybeTempDouble_; }
    FloatRegister maybeTempFloat32() const { return maybeTempFloat32_; }

    bool strict() const { return strict_; }
    bool needsPostBarrier() const { return needsPostBarrier_; }
    bool needsTypeBarrier() const { return needsTypeBarrier_; }
    bool guardHoles() const { return guardHoles_; }

    static MOZ_MUST_USE bool update(JSContext* cx, HandleScript outerScript, IonSetPropertyIC* ic,
                                    HandleObject obj, HandleValue idVal, HandleValue rhs);
};

class IonGetNameIC : public IonIC
{
    LiveRegisterSet liveRegs_;

    Register environment_;
    ValueOperand output_;
    Register temp_;

  public:
    IonGetNameIC(LiveRegisterSet liveRegs, Register environment, ValueOperand output,
                 Register temp)
      : IonIC(CacheKind::GetName),
        liveRegs_(liveRegs),
        environment_(environment),
        output_(output),
        temp_(temp)
    { }

    Register environment() const { return environment_; }
    ValueOperand output() const { return output_; }
    Register temp() const { return temp_; }
    LiveRegisterSet liveRegs() const { return liveRegs_; }

    static MOZ_MUST_USE bool update(JSContext* cx, HandleScript outerScript, IonGetNameIC* ic,
                                    HandleObject envChain, MutableHandleValue res);
};

class IonBindNameIC : public IonIC
{
    LiveRegisterSet liveRegs_;

    Register environment_;
    Register output_;
    Register temp_;

  public:
    IonBindNameIC(LiveRegisterSet liveRegs, Register environment, Register output, Register temp)
      : IonIC(CacheKind::BindName),
        liveRegs_(liveRegs),
        environment_(environment),
        output_(output),
        temp_(temp)
    { }

    Register environment() const { return environment_; }
    Register output() const { return output_; }
    Register temp() const { return temp_; }
    LiveRegisterSet liveRegs() const { return liveRegs_; }

    static JSObject* update(JSContext* cx, HandleScript outerScript, IonBindNameIC* ic,
                            HandleObject envChain);
};

class IonHasOwnIC : public IonIC
{
    LiveRegisterSet liveRegs_;

    TypedOrValueRegister value_;
    TypedOrValueRegister id_;
    Register output_;

  public:
    IonHasOwnIC(LiveRegisterSet liveRegs, TypedOrValueRegister value,
                TypedOrValueRegister id, Register output)
      : IonIC(CacheKind::HasOwn),
        liveRegs_(liveRegs),
        value_(value),
        id_(id),
        output_(output)
    { }

    TypedOrValueRegister value() const { return value_; }
    TypedOrValueRegister id() const { return id_; }
    Register output() const { return output_; }
    LiveRegisterSet liveRegs() const { return liveRegs_; }

    static MOZ_MUST_USE bool update(JSContext* cx, HandleScript outerScript, IonHasOwnIC* ic,
                                    HandleValue val, HandleValue idVal, int32_t* res);
};

} // namespace jit
} // namespace js

#endif /* jit_IonIC_h */
