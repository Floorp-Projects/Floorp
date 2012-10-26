/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_jit_h__) && defined(JS_ION)
#define jsion_baseline_jit_h__

#include "jscntxt.h"
#include "jscompartment.h"
#include "IonCode.h"
#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

// Since BaseCache has virtual methods, it can't be stored in a Vector.
// As a workaround, use a POD base class to hold all the data.
struct CacheData
{
    jsbytecode *pc;
    uint32_t data;
    CodeLocationCall call;

    void updateBaseAddress(IonCode *code, MacroAssembler &masm) {
        call.repoint(code, &masm);
    }
};

class BaselineScript;

class BaseCache
{
  public:
    CacheData &data;

    explicit BaseCache(CacheData &data)
      : data(data)
    { }

    explicit BaseCache(CacheData &data, jsbytecode *pc)
      : data(data)
    {
        data.pc = pc;
    }

  protected:
    virtual uint32_t getKey() const = 0;
    virtual IonCode *generate(JSContext *cx) = 0;

  public:
    IonCode *getCode(JSContext *cx) {
        uint32_t key = getKey();
        (void)key; //XXX

        // XXX if lookup failure..
        return generate(cx);
    }
};

class BinaryOpCache : public BaseCache
{
    enum State {
        Uninitialized = 0,
        Int32
    };

  public:
    explicit BinaryOpCache(CacheData &data)
      : BaseCache(data)
    { }

    explicit BinaryOpCache(CacheData &data, jsbytecode *pc)
      : BaseCache(data, pc)
    {
        data.pc = pc;
        setState(Uninitialized);
    }

    uint32_t getKey() const {
        return uint32_t(JSOp(*data.pc) << 16) | data.data;
    }

    bool update(JSContext *cx, HandleValue lhs, HandleValue rhs, HandleValue res);

    IonCode *generate(JSContext *cx);
    bool generateUpdate(JSContext *cx, MacroAssembler &masm);
    bool generateInt32(JSContext *cx, MacroAssembler &masm);

    State getTargetState(HandleValue lhs, HandleValue rhs, HandleValue res);

    State getState() const {
        return (State)data.data;
    }
    void setState(State state) {
        data.data = (uint32_t)state;
    }
};

class CompareCache : public BaseCache
{
    enum State {
        Uninitialized = 0,
        Int32
    };

  public:
    explicit CompareCache(CacheData &data)
      : BaseCache(data)
    { }

    explicit CompareCache(CacheData &data, jsbytecode *pc)
      : BaseCache(data, pc)
    {
        data.pc = pc;
        setState(Uninitialized);
    }

    uint32_t getKey() const {
        return uint32_t(JSOp(*data.pc) << 16) | data.data;
    }

    bool update(JSContext *cx, HandleValue lhs, HandleValue rhs);

    IonCode *generate(JSContext *cx);
    bool generateUpdate(JSContext *cx, MacroAssembler &masm);
    bool generateInt32(JSContext *cx, MacroAssembler &masm);

    State getTargetState(HandleValue lhs, HandleValue rhs);

    State getState() const {
        return (State)data.data;
    }
    void setState(State state) {
        data.data = (uint32_t)state;
    }
};

struct BaselineScript
{
  private:
    // Code pointer containing the actual method.
    HeapPtr<IonCode> method_;

  private:
    void trace(JSTracer *trc);

    uint32 cacheList_;
    uint32 cacheEntries_;

  public:
    // Do not call directly, use BaselineScript::New. This is public for cx->new_.
    BaselineScript();

    static BaselineScript *New(JSContext *cx, size_t cacheEntries);
    static void Trace(JSTracer *trc, BaselineScript *script);
    static void Destroy(FreeOp *fop, BaselineScript *script);

    static inline size_t offsetOfMethod() {
        return offsetof(BaselineScript, method_);
    }

    CacheData *cacheList() {
        return (CacheData *)(reinterpret_cast<uint8 *>(this) + cacheList_);
    }

    IonCode *method() const {
        return method_;
    }
    void setMethod(IonCode *code) {
        //JS_ASSERT(!invalidated());
        method_ = code;
    }

    CacheData &getCache(size_t index) {
        JS_ASSERT(index < numCaches());
        return cacheList()[index];
    }

    CacheData &cacheDataFromReturnAddr(uint8_t *addr);

    size_t numCaches() const {
        return cacheEntries_;
    }

    void copyCacheEntries(const CacheData *caches, MacroAssembler &masm);
};

MethodStatus
CanEnterBaselineJIT(JSContext *cx, HandleScript script, StackFrame *fp);

IonExecStatus
EnterBaselineMethod(JSContext *cx, StackFrame *fp);

} // namespace ion
} // namespace js

#endif

