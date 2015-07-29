/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/SharedIC.h"
#include "mozilla/SizePrintfMacros.h"

#include "jstypes.h"

#include "jit/BaselineIC.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"

namespace js {
namespace jit {

#ifdef DEBUG
void
FallbackICSpew(JSContext* cx, ICFallbackStub* stub, const char* fmt, ...)
{
    if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
        RootedScript script(cx, GetTopJitJSScript(cx));
        jsbytecode* pc = stub->icEntry()->pc(script);

        char fmtbuf[100];
        va_list args;
        va_start(args, fmt);
        vsnprintf(fmtbuf, 100, fmt, args);
        va_end(args);

        JitSpew(JitSpew_BaselineICFallback,
                "Fallback hit for (%s:%" PRIuSIZE ") (pc=%" PRIuSIZE ",line=%d,uses=%d,stubs=%" PRIuSIZE "): %s",
                script->filename(),
                script->lineno(),
                script->pcToOffset(pc),
                PCToLineNumber(script, pc),
                script->getWarmUpCount(),
                stub->numOptimizedStubs(),
                fmtbuf);
    }
}

void
TypeFallbackICSpew(JSContext* cx, ICTypeMonitor_Fallback* stub, const char* fmt, ...)
{
    if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
        RootedScript script(cx, GetTopJitJSScript(cx));
        jsbytecode* pc = stub->icEntry()->pc(script);

        char fmtbuf[100];
        va_list args;
        va_start(args, fmt);
        vsnprintf(fmtbuf, 100, fmt, args);
        va_end(args);

        JitSpew(JitSpew_BaselineICFallback,
                "Type monitor fallback hit for (%s:%" PRIuSIZE ") (pc=%" PRIuSIZE ",line=%d,uses=%d,stubs=%d): %s",
                script->filename(),
                script->lineno(),
                script->pcToOffset(pc),
                PCToLineNumber(script, pc),
                script->getWarmUpCount(),
                (int) stub->numOptimizedMonitorStubs(),
                fmtbuf);
    }
}
#endif

ICFallbackStub*
ICEntry::fallbackStub() const
{
    return firstStub()->getChainFallback();
}


ICStubConstIterator&
ICStubConstIterator::operator++()
{
    MOZ_ASSERT(currentStub_ != nullptr);
    currentStub_ = currentStub_->next();
    return *this;
}


ICStubIterator::ICStubIterator(ICFallbackStub* fallbackStub, bool end)
  : icEntry_(fallbackStub->icEntry()),
    fallbackStub_(fallbackStub),
    previousStub_(nullptr),
    currentStub_(end ? fallbackStub : icEntry_->firstStub()),
    unlinked_(false)
{ }

ICStubIterator&
ICStubIterator::operator++()
{
    MOZ_ASSERT(currentStub_->next() != nullptr);
    if (!unlinked_)
        previousStub_ = currentStub_;
    currentStub_ = currentStub_->next();
    unlinked_ = false;
    return *this;
}

void
ICStubIterator::unlink(JSContext* cx)
{
    MOZ_ASSERT(currentStub_->next() != nullptr);
    MOZ_ASSERT(currentStub_ != fallbackStub_);
    MOZ_ASSERT(!unlinked_);

    fallbackStub_->unlinkStub(cx->zone(), previousStub_, currentStub_);

    // Mark the current iterator position as unlinked, so operator++ works properly.
    unlinked_ = true;
}


void
ICStub::markCode(JSTracer* trc, const char* name)
{
    JitCode* stubJitCode = jitCode();
    TraceManuallyBarrieredEdge(trc, &stubJitCode, name);
}

void
ICStub::updateCode(JitCode* code)
{
    // Write barrier on the old code.
    JitCode::writeBarrierPre(jitCode());
    stubCode_ = code->raw();
}

/* static */ void
ICStub::trace(JSTracer* trc)
{
    markCode(trc, "baseline-stub-jitcode");

    // If the stub is a monitored fallback stub, then mark the monitor ICs hanging
    // off of that stub.  We don't need to worry about the regular monitored stubs,
    // because the regular monitored stubs will always have a monitored fallback stub
    // that references the same stub chain.
    if (isMonitoredFallback()) {
        ICTypeMonitor_Fallback* lastMonStub = toMonitoredFallbackStub()->fallbackMonitorStub();
        for (ICStubConstIterator iter(lastMonStub->firstMonitorStub()); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->next() == nullptr, *iter == lastMonStub);
            iter->trace(trc);
        }
    }

    if (isUpdated()) {
        for (ICStubConstIterator iter(toUpdatedStub()->firstUpdateStub()); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->next() == nullptr, iter->isTypeUpdate_Fallback());
            iter->trace(trc);
        }
    }

    switch (kind()) {
      case ICStub::Call_Scripted: {
        ICCall_Scripted* callStub = toCall_Scripted();
        TraceEdge(trc, &callStub->callee(), "baseline-callscripted-callee");
        if (callStub->templateObject())
            TraceEdge(trc, &callStub->templateObject(), "baseline-callscripted-template");
        break;
      }
      case ICStub::Call_Native: {
        ICCall_Native* callStub = toCall_Native();
        TraceEdge(trc, &callStub->callee(), "baseline-callnative-callee");
        if (callStub->templateObject())
            TraceEdge(trc, &callStub->templateObject(), "baseline-callnative-template");
        break;
      }
      case ICStub::Call_ClassHook: {
        ICCall_ClassHook* callStub = toCall_ClassHook();
        if (callStub->templateObject())
            TraceEdge(trc, &callStub->templateObject(), "baseline-callclasshook-template");
        break;
      }
      case ICStub::Call_StringSplit: {
        ICCall_StringSplit* callStub = toCall_StringSplit();
        TraceEdge(trc, &callStub->templateObject(), "baseline-callstringsplit-template");
        TraceEdge(trc, &callStub->expectedArg(), "baseline-callstringsplit-arg");
        TraceEdge(trc, &callStub->expectedThis(), "baseline-callstringsplit-this");
        break;
      }
      case ICStub::GetElem_NativeSlotName:
      case ICStub::GetElem_NativeSlotSymbol:
      case ICStub::GetElem_UnboxedPropertyName: {
        ICGetElemNativeStub* getElemStub = static_cast<ICGetElemNativeStub*>(this);
        getElemStub->receiverGuard().trace(trc);
        if (getElemStub->isSymbol()) {
            ICGetElem_NativeSlot<JS::Symbol*>* typedGetElemStub = toGetElem_NativeSlotSymbol();
            TraceEdge(trc, &typedGetElemStub->key(), "baseline-getelem-native-key");
        } else {
            ICGetElemNativeSlotStub<PropertyName*>* typedGetElemStub =
                reinterpret_cast<ICGetElemNativeSlotStub<PropertyName*>*>(this);
            TraceEdge(trc, &typedGetElemStub->key(), "baseline-getelem-native-key");
        }
        break;
      }
      case ICStub::GetElem_NativePrototypeSlotName:
      case ICStub::GetElem_NativePrototypeSlotSymbol: {
        ICGetElemNativeStub* getElemStub = static_cast<ICGetElemNativeStub*>(this);
        getElemStub->receiverGuard().trace(trc);
        if (getElemStub->isSymbol()) {
            ICGetElem_NativePrototypeSlot<JS::Symbol*>* typedGetElemStub
                = toGetElem_NativePrototypeSlotSymbol();
            TraceEdge(trc, &typedGetElemStub->key(), "baseline-getelem-nativeproto-key");
            TraceEdge(trc, &typedGetElemStub->holder(), "baseline-getelem-nativeproto-holder");
            TraceEdge(trc, &typedGetElemStub->holderShape(), "baseline-getelem-nativeproto-holdershape");
        } else {
            ICGetElem_NativePrototypeSlot<PropertyName*>* typedGetElemStub
                = toGetElem_NativePrototypeSlotName();
            TraceEdge(trc, &typedGetElemStub->key(), "baseline-getelem-nativeproto-key");
            TraceEdge(trc, &typedGetElemStub->holder(), "baseline-getelem-nativeproto-holder");
            TraceEdge(trc, &typedGetElemStub->holderShape(), "baseline-getelem-nativeproto-holdershape");
        }
        break;
      }
      case ICStub::GetElem_NativePrototypeCallNativeName:
      case ICStub::GetElem_NativePrototypeCallNativeSymbol:
      case ICStub::GetElem_NativePrototypeCallScriptedName:
      case ICStub::GetElem_NativePrototypeCallScriptedSymbol: {
        ICGetElemNativeStub* getElemStub = static_cast<ICGetElemNativeStub*>(this);
        getElemStub->receiverGuard().trace(trc);
        if (getElemStub->isSymbol()) {
            ICGetElemNativePrototypeCallStub<JS::Symbol*>* callStub =
                reinterpret_cast<ICGetElemNativePrototypeCallStub<JS::Symbol*>*>(this);
            TraceEdge(trc, &callStub->key(), "baseline-getelem-nativeprotocall-key");
            TraceEdge(trc, &callStub->getter(), "baseline-getelem-nativeprotocall-getter");
            TraceEdge(trc, &callStub->holder(), "baseline-getelem-nativeprotocall-holder");
            TraceEdge(trc, &callStub->holderShape(), "baseline-getelem-nativeprotocall-holdershape");
        } else {
            ICGetElemNativePrototypeCallStub<PropertyName*>* callStub =
                reinterpret_cast<ICGetElemNativePrototypeCallStub<PropertyName*>*>(this);
            TraceEdge(trc, &callStub->key(), "baseline-getelem-nativeprotocall-key");
            TraceEdge(trc, &callStub->getter(), "baseline-getelem-nativeprotocall-getter");
            TraceEdge(trc, &callStub->holder(), "baseline-getelem-nativeprotocall-holder");
            TraceEdge(trc, &callStub->holderShape(), "baseline-getelem-nativeprotocall-holdershape");
        }
        break;
      }
      case ICStub::GetElem_Dense: {
        ICGetElem_Dense* getElemStub = toGetElem_Dense();
        TraceEdge(trc, &getElemStub->shape(), "baseline-getelem-dense-shape");
        break;
      }
      case ICStub::GetElem_UnboxedArray: {
        ICGetElem_UnboxedArray* getElemStub = toGetElem_UnboxedArray();
        TraceEdge(trc, &getElemStub->group(), "baseline-getelem-unboxed-array-group");
        break;
      }
      case ICStub::GetElem_TypedArray: {
        ICGetElem_TypedArray* getElemStub = toGetElem_TypedArray();
        TraceEdge(trc, &getElemStub->shape(), "baseline-getelem-typedarray-shape");
        break;
      }
      case ICStub::SetElem_DenseOrUnboxedArray: {
        ICSetElem_DenseOrUnboxedArray* setElemStub = toSetElem_DenseOrUnboxedArray();
        if (setElemStub->shape())
            TraceEdge(trc, &setElemStub->shape(), "baseline-getelem-dense-shape");
        TraceEdge(trc, &setElemStub->group(), "baseline-setelem-dense-group");
        break;
      }
      case ICStub::SetElem_DenseOrUnboxedArrayAdd: {
        ICSetElem_DenseOrUnboxedArrayAdd* setElemStub = toSetElem_DenseOrUnboxedArrayAdd();
        TraceEdge(trc, &setElemStub->group(), "baseline-setelem-denseadd-group");

        JS_STATIC_ASSERT(ICSetElem_DenseOrUnboxedArrayAdd::MAX_PROTO_CHAIN_DEPTH == 4);

        switch (setElemStub->protoChainDepth()) {
          case 0: setElemStub->toImpl<0>()->traceShapes(trc); break;
          case 1: setElemStub->toImpl<1>()->traceShapes(trc); break;
          case 2: setElemStub->toImpl<2>()->traceShapes(trc); break;
          case 3: setElemStub->toImpl<3>()->traceShapes(trc); break;
          case 4: setElemStub->toImpl<4>()->traceShapes(trc); break;
          default: MOZ_CRASH("Invalid proto stub.");
        }
        break;
      }
      case ICStub::SetElem_TypedArray: {
        ICSetElem_TypedArray* setElemStub = toSetElem_TypedArray();
        TraceEdge(trc, &setElemStub->shape(), "baseline-setelem-typedarray-shape");
        break;
      }
      case ICStub::TypeMonitor_SingleObject: {
        ICTypeMonitor_SingleObject* monitorStub = toTypeMonitor_SingleObject();
        TraceEdge(trc, &monitorStub->object(), "baseline-monitor-singleton");
        break;
      }
      case ICStub::TypeMonitor_ObjectGroup: {
        ICTypeMonitor_ObjectGroup* monitorStub = toTypeMonitor_ObjectGroup();
        TraceEdge(trc, &monitorStub->group(), "baseline-monitor-group");
        break;
      }
      case ICStub::TypeUpdate_SingleObject: {
        ICTypeUpdate_SingleObject* updateStub = toTypeUpdate_SingleObject();
        TraceEdge(trc, &updateStub->object(), "baseline-update-singleton");
        break;
      }
      case ICStub::TypeUpdate_ObjectGroup: {
        ICTypeUpdate_ObjectGroup* updateStub = toTypeUpdate_ObjectGroup();
        TraceEdge(trc, &updateStub->group(), "baseline-update-group");
        break;
      }
      case ICStub::In_Native: {
        ICIn_Native* inStub = toIn_Native();
        TraceEdge(trc, &inStub->shape(), "baseline-innative-stub-shape");
        TraceEdge(trc, &inStub->name(), "baseline-innative-stub-name");
        break;
      }
      case ICStub::In_NativePrototype: {
        ICIn_NativePrototype* inStub = toIn_NativePrototype();
        TraceEdge(trc, &inStub->shape(), "baseline-innativeproto-stub-shape");
        TraceEdge(trc, &inStub->name(), "baseline-innativeproto-stub-name");
        TraceEdge(trc, &inStub->holder(), "baseline-innativeproto-stub-holder");
        TraceEdge(trc, &inStub->holderShape(), "baseline-innativeproto-stub-holdershape");
        break;
      }
      case ICStub::In_NativeDoesNotExist: {
        ICIn_NativeDoesNotExist* inStub = toIn_NativeDoesNotExist();
        TraceEdge(trc, &inStub->name(), "baseline-innativedoesnotexist-stub-name");
        JS_STATIC_ASSERT(ICIn_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH == 8);
        switch (inStub->protoChainDepth()) {
          case 0: inStub->toImpl<0>()->traceShapes(trc); break;
          case 1: inStub->toImpl<1>()->traceShapes(trc); break;
          case 2: inStub->toImpl<2>()->traceShapes(trc); break;
          case 3: inStub->toImpl<3>()->traceShapes(trc); break;
          case 4: inStub->toImpl<4>()->traceShapes(trc); break;
          case 5: inStub->toImpl<5>()->traceShapes(trc); break;
          case 6: inStub->toImpl<6>()->traceShapes(trc); break;
          case 7: inStub->toImpl<7>()->traceShapes(trc); break;
          case 8: inStub->toImpl<8>()->traceShapes(trc); break;
          default: MOZ_CRASH("Invalid proto stub.");
        }
        break;
      }
      case ICStub::In_Dense: {
        ICIn_Dense* inStub = toIn_Dense();
        TraceEdge(trc, &inStub->shape(), "baseline-in-dense-shape");
        break;
      }
      case ICStub::GetName_Global: {
        ICGetName_Global* globalStub = toGetName_Global();
        TraceEdge(trc, &globalStub->shape(), "baseline-global-stub-shape");
        break;
      }
      case ICStub::GetName_Scope0:
        static_cast<ICGetName_Scope<0>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetName_Scope1:
        static_cast<ICGetName_Scope<1>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetName_Scope2:
        static_cast<ICGetName_Scope<2>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetName_Scope3:
        static_cast<ICGetName_Scope<3>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetName_Scope4:
        static_cast<ICGetName_Scope<4>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetName_Scope5:
        static_cast<ICGetName_Scope<5>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetName_Scope6:
        static_cast<ICGetName_Scope<6>*>(this)->traceScopes(trc);
        break;
      case ICStub::GetIntrinsic_Constant: {
        ICGetIntrinsic_Constant* constantStub = toGetIntrinsic_Constant();
        TraceEdge(trc, &constantStub->value(), "baseline-getintrinsic-constant-value");
        break;
      }
      case ICStub::GetProp_Primitive: {
        ICGetProp_Primitive* propStub = toGetProp_Primitive();
        TraceEdge(trc, &propStub->protoShape(), "baseline-getprop-primitive-stub-shape");
        break;
      }
      case ICStub::GetProp_Native: {
        ICGetProp_Native* propStub = toGetProp_Native();
        propStub->receiverGuard().trace(trc);
        break;
      }
      case ICStub::GetProp_NativePrototype: {
        ICGetProp_NativePrototype* propStub = toGetProp_NativePrototype();
        propStub->receiverGuard().trace(trc);
        TraceEdge(trc, &propStub->holder(), "baseline-getpropnativeproto-stub-holder");
        TraceEdge(trc, &propStub->holderShape(), "baseline-getpropnativeproto-stub-holdershape");
        break;
      }
      case ICStub::GetProp_NativeDoesNotExist: {
        ICGetProp_NativeDoesNotExist* propStub = toGetProp_NativeDoesNotExist();
        propStub->guard().trace(trc);
        JS_STATIC_ASSERT(ICGetProp_NativeDoesNotExist::MAX_PROTO_CHAIN_DEPTH == 8);
        switch (propStub->protoChainDepth()) {
          case 0: propStub->toImpl<0>()->traceShapes(trc); break;
          case 1: propStub->toImpl<1>()->traceShapes(trc); break;
          case 2: propStub->toImpl<2>()->traceShapes(trc); break;
          case 3: propStub->toImpl<3>()->traceShapes(trc); break;
          case 4: propStub->toImpl<4>()->traceShapes(trc); break;
          case 5: propStub->toImpl<5>()->traceShapes(trc); break;
          case 6: propStub->toImpl<6>()->traceShapes(trc); break;
          case 7: propStub->toImpl<7>()->traceShapes(trc); break;
          case 8: propStub->toImpl<8>()->traceShapes(trc); break;
          default: MOZ_CRASH("Invalid proto stub.");
        }
        break;
      }
      case ICStub::GetProp_Unboxed: {
        ICGetProp_Unboxed* propStub = toGetProp_Unboxed();
        TraceEdge(trc, &propStub->group(), "baseline-getprop-unboxed-stub-group");
        break;
      }
      case ICStub::GetProp_TypedObject: {
        ICGetProp_TypedObject* propStub = toGetProp_TypedObject();
        TraceEdge(trc, &propStub->shape(), "baseline-getprop-typedobject-stub-shape");
        break;
      }
      case ICStub::GetProp_CallDOMProxyNative:
      case ICStub::GetProp_CallDOMProxyWithGenerationNative: {
        ICGetPropCallDOMProxyNativeStub* propStub;
        if (kind() == ICStub::GetProp_CallDOMProxyNative)
            propStub = toGetProp_CallDOMProxyNative();
        else
            propStub = toGetProp_CallDOMProxyWithGenerationNative();
        propStub->receiverGuard().trace(trc);
        if (propStub->expandoShape()) {
            TraceEdge(trc, &propStub->expandoShape(),
                      "baseline-getproplistbasenative-stub-expandoshape");
        }
        TraceEdge(trc, &propStub->holder(), "baseline-getproplistbasenative-stub-holder");
        TraceEdge(trc, &propStub->holderShape(), "baseline-getproplistbasenative-stub-holdershape");
        TraceEdge(trc, &propStub->getter(), "baseline-getproplistbasenative-stub-getter");
        break;
      }
      case ICStub::GetProp_DOMProxyShadowed: {
        ICGetProp_DOMProxyShadowed* propStub = toGetProp_DOMProxyShadowed();
        TraceEdge(trc, &propStub->shape(), "baseline-getproplistbaseshadowed-stub-shape");
        TraceEdge(trc, &propStub->name(), "baseline-getproplistbaseshadowed-stub-name");
        break;
      }
      case ICStub::GetProp_CallScripted: {
        ICGetProp_CallScripted* callStub = toGetProp_CallScripted();
        callStub->receiverGuard().trace(trc);
        TraceEdge(trc, &callStub->holder(), "baseline-getpropcallscripted-stub-holder");
        TraceEdge(trc, &callStub->holderShape(), "baseline-getpropcallscripted-stub-holdershape");
        TraceEdge(trc, &callStub->getter(), "baseline-getpropcallscripted-stub-getter");
        break;
      }
      case ICStub::GetProp_CallNative: {
        ICGetProp_CallNative* callStub = toGetProp_CallNative();
        callStub->receiverGuard().trace(trc);
        TraceEdge(trc, &callStub->holder(), "baseline-getpropcallnative-stub-holder");
        TraceEdge(trc, &callStub->holderShape(), "baseline-getpropcallnative-stub-holdershape");
        TraceEdge(trc, &callStub->getter(), "baseline-getpropcallnative-stub-getter");
        break;
      }
      case ICStub::SetProp_Native: {
        ICSetProp_Native* propStub = toSetProp_Native();
        TraceEdge(trc, &propStub->shape(), "baseline-setpropnative-stub-shape");
        TraceEdge(trc, &propStub->group(), "baseline-setpropnative-stub-group");
        break;
      }
      case ICStub::SetProp_NativeAdd: {
        ICSetProp_NativeAdd* propStub = toSetProp_NativeAdd();
        TraceEdge(trc, &propStub->group(), "baseline-setpropnativeadd-stub-group");
        TraceEdge(trc, &propStub->newShape(), "baseline-setpropnativeadd-stub-newshape");
        if (propStub->newGroup())
            TraceEdge(trc, &propStub->newGroup(), "baseline-setpropnativeadd-stub-new-group");
        JS_STATIC_ASSERT(ICSetProp_NativeAdd::MAX_PROTO_CHAIN_DEPTH == 4);
        switch (propStub->protoChainDepth()) {
          case 0: propStub->toImpl<0>()->traceShapes(trc); break;
          case 1: propStub->toImpl<1>()->traceShapes(trc); break;
          case 2: propStub->toImpl<2>()->traceShapes(trc); break;
          case 3: propStub->toImpl<3>()->traceShapes(trc); break;
          case 4: propStub->toImpl<4>()->traceShapes(trc); break;
          default: MOZ_CRASH("Invalid proto stub.");
        }
        break;
      }
      case ICStub::SetProp_Unboxed: {
        ICSetProp_Unboxed* propStub = toSetProp_Unboxed();
        TraceEdge(trc, &propStub->group(), "baseline-setprop-unboxed-stub-group");
        break;
      }
      case ICStub::SetProp_TypedObject: {
        ICSetProp_TypedObject* propStub = toSetProp_TypedObject();
        TraceEdge(trc, &propStub->shape(), "baseline-setprop-typedobject-stub-shape");
        TraceEdge(trc, &propStub->group(), "baseline-setprop-typedobject-stub-group");
        break;
      }
      case ICStub::SetProp_CallScripted: {
        ICSetProp_CallScripted* callStub = toSetProp_CallScripted();
        callStub->receiverGuard().trace(trc);
        TraceEdge(trc, &callStub->holder(), "baseline-setpropcallscripted-stub-holder");
        TraceEdge(trc, &callStub->holderShape(), "baseline-setpropcallscripted-stub-holdershape");
        TraceEdge(trc, &callStub->setter(), "baseline-setpropcallscripted-stub-setter");
        break;
      }
      case ICStub::SetProp_CallNative: {
        ICSetProp_CallNative* callStub = toSetProp_CallNative();
        callStub->receiverGuard().trace(trc);
        TraceEdge(trc, &callStub->holder(), "baseline-setpropcallnative-stub-holder");
        TraceEdge(trc, &callStub->holderShape(), "baseline-setpropcallnative-stub-holdershape");
        TraceEdge(trc, &callStub->setter(), "baseline-setpropcallnative-stub-setter");
        break;
      }
      case ICStub::InstanceOf_Function: {
        ICInstanceOf_Function* instanceofStub = toInstanceOf_Function();
        TraceEdge(trc, &instanceofStub->shape(), "baseline-instanceof-fun-shape");
        TraceEdge(trc, &instanceofStub->prototypeObject(), "baseline-instanceof-fun-prototype");
        break;
      }
      case ICStub::NewArray_Fallback: {
        ICNewArray_Fallback* stub = toNewArray_Fallback();
        if (stub->templateObject())
            TraceEdge(trc, &stub->templateObject(), "baseline-newarray-template");
        TraceEdge(trc, &stub->templateGroup(), "baseline-newarray-template-group");
        break;
      }
      case ICStub::NewObject_Fallback: {
        ICNewObject_Fallback* stub = toNewObject_Fallback();
        if (stub->templateObject())
            TraceEdge(trc, &stub->templateObject(), "baseline-newobject-template");
        break;
      }
      case ICStub::Rest_Fallback: {
        ICRest_Fallback* stub = toRest_Fallback();
        TraceEdge(trc, &stub->templateObject(), "baseline-rest-template");
        break;
      }
      default:
        break;
    }
}

void
ICFallbackStub::unlinkStub(Zone* zone, ICStub* prev, ICStub* stub)
{
    MOZ_ASSERT(stub->next());

    // If stub is the last optimized stub, update lastStubPtrAddr.
    if (stub->next() == this) {
        MOZ_ASSERT(lastStubPtrAddr_ == stub->addressOfNext());
        if (prev)
            lastStubPtrAddr_ = prev->addressOfNext();
        else
            lastStubPtrAddr_ = icEntry()->addressOfFirstStub();
        *lastStubPtrAddr_ = this;
    } else {
        if (prev) {
            MOZ_ASSERT(prev->next() == stub);
            prev->setNext(stub->next());
        } else {
            MOZ_ASSERT(icEntry()->firstStub() == stub);
            icEntry()->setFirstStub(stub->next());
        }
    }

    MOZ_ASSERT(numOptimizedStubs_ > 0);
    numOptimizedStubs_--;

    if (zone->needsIncrementalBarrier()) {
        // We are removing edges from ICStub to gcthings. Perform one final trace
        // of the stub for incremental GC, as it must know about those edges.
        stub->trace(zone->barrierTracer());
    }

    if (ICStub::CanMakeCalls(stub->kind()) && stub->isMonitored()) {
        // This stub can make calls so we can return to it if it's on the stack.
        // We just have to reset its firstMonitorStub_ field to avoid a stale
        // pointer when purgeOptimizedStubs destroys all optimized monitor
        // stubs (unlinked stubs won't be updated).
        ICTypeMonitor_Fallback* monitorFallback = toMonitoredFallbackStub()->fallbackMonitorStub();
        stub->toMonitoredStub()->resetFirstMonitorStub(monitorFallback);
    }

#ifdef DEBUG
    // Poison stub code to ensure we don't call this stub again. However, if this
    // stub can make calls, a pointer to it may be stored in a stub frame on the
    // stack, so we can't touch the stubCode_ or GC will crash when marking this
    // pointer.
    if (!ICStub::CanMakeCalls(stub->kind()))
        stub->stubCode_ = (uint8_t*)0xbad;
#endif
}

void
ICFallbackStub::unlinkStubsWithKind(JSContext* cx, ICStub::Kind kind)
{
    for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++) {
        if (iter->kind() == kind)
            iter.unlink(cx);
    }
}

void
ICTypeMonitor_Fallback::resetMonitorStubChain(Zone* zone)
{
    if (zone->needsIncrementalBarrier()) {
        // We are removing edges from monitored stubs to gcthings (JitCode).
        // Perform one final trace of all monitor stubs for incremental GC,
        // as it must know about those edges.
        for (ICStub* s = firstMonitorStub_; !s->isTypeMonitor_Fallback(); s = s->next())
            s->trace(zone->barrierTracer());
    }

    firstMonitorStub_ = this;
    numOptimizedMonitorStubs_ = 0;

    if (hasFallbackStub_) {
        lastMonitorStubPtrAddr_ = nullptr;

        // Reset firstMonitorStub_ field of all monitored stubs.
        for (ICStubConstIterator iter = mainFallbackStub_->beginChainConst();
             !iter.atEnd(); iter++)
        {
            if (!iter->isMonitored())
                continue;
            iter->toMonitoredStub()->resetFirstMonitorStub(this);
        }
    } else {
        icEntry_->setFirstStub(this);
        lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }
}

ICMonitoredStub::ICMonitoredStub(Kind kind, JitCode* stubCode, ICStub* firstMonitorStub)
  : ICStub(kind, ICStub::Monitored, stubCode),
    firstMonitorStub_(firstMonitorStub)
{
    // If the first monitored stub is a ICTypeMonitor_Fallback stub, then
    // double check that _its_ firstMonitorStub is the same as this one.
    MOZ_ASSERT_IF(firstMonitorStub_->isTypeMonitor_Fallback(),
                  firstMonitorStub_->toTypeMonitor_Fallback()->firstMonitorStub() ==
                     firstMonitorStub_);
}

bool
ICMonitoredFallbackStub::initMonitoringChain(JSContext* cx, ICStubSpace* space)
{
    MOZ_ASSERT(fallbackMonitorStub_ == nullptr);

    ICTypeMonitor_Fallback::Compiler compiler(cx, this);
    ICTypeMonitor_Fallback* stub = compiler.getStub(space);
    if (!stub)
        return false;
    fallbackMonitorStub_ = stub;
    return true;
}

bool
ICMonitoredFallbackStub::addMonitorStubForValue(JSContext* cx, JSScript* script, HandleValue val)
{
    return fallbackMonitorStub_->addMonitorStubForValue(cx, script, val);
}

bool
ICUpdatedStub::initUpdatingChain(JSContext* cx, ICStubSpace* space)
{
    MOZ_ASSERT(firstUpdateStub_ == nullptr);

    ICTypeUpdate_Fallback::Compiler compiler(cx);
    ICTypeUpdate_Fallback* stub = compiler.getStub(space);
    if (!stub)
        return false;

    firstUpdateStub_ = stub;
    return true;
}

JitCode*
ICStubCompiler::getStubCode()
{
    JitCompartment* comp = cx->compartment()->jitCompartment();

    // Check for existing cached stubcode.
    uint32_t stubKey = getKey();
    JitCode* stubCode = comp->getStubCode(stubKey);
    if (stubCode)
        return stubCode;

    // Compile new stubcode.
    JitContext jctx(cx, nullptr);
    MacroAssembler masm;
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    if (!generateStubCode(masm))
        return nullptr;
    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx, linker.newCode<CanGC>(cx, BASELINE_CODE));
    if (!newStubCode)
        return nullptr;

    // After generating code, run postGenerateStubCode()
    if (!postGenerateStubCode(masm, newStubCode))
        return nullptr;

    // All barriers are emitted off-by-default, enable them if needed.
    if (cx->zone()->needsIncrementalBarrier())
        newStubCode->togglePreBarriers(true);

    // Cache newly compiled stubcode.
    if (!comp->putStubCode(stubKey, newStubCode))
        return nullptr;

    MOZ_ASSERT(entersStubFrame_ == ICStub::CanMakeCalls(kind));
    MOZ_ASSERT(!inStubFrame_);

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(newStubCode, "BaselineIC");
#endif

    return newStubCode;
}

bool
ICStubCompiler::tailCallVM(const VMFunction& fun, MacroAssembler& masm)
{
    JitCode* code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

    MOZ_ASSERT(fun.expectTailCall == TailCall);
    uint32_t argSize = fun.explicitStackSlots() * sizeof(void*);
    EmitTailCallVM(code, masm, argSize);
    return true;
}

bool
ICStubCompiler::callVM(const VMFunction& fun, MacroAssembler& masm)
{
    MOZ_ASSERT(inStubFrame_);

    JitCode* code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

    MOZ_ASSERT(fun.expectTailCall == NonTailCall);
    EmitCallVM(code, masm);
    return true;
}

bool
ICStubCompiler::callTypeUpdateIC(MacroAssembler& masm, uint32_t objectOffset)
{
    JitCode* code = cx->runtime()->jitRuntime()->getVMWrapper(DoTypeUpdateFallbackInfo);
    if (!code)
        return false;

    EmitCallTypeUpdateIC(masm, code, objectOffset);
    return true;
}

void
ICStubCompiler::enterStubFrame(MacroAssembler& masm, Register scratch)
{
    EmitEnterStubFrame(masm, scratch);

    MOZ_ASSERT(!inStubFrame_);
    inStubFrame_ = true;

#ifdef DEBUG
    entersStubFrame_ = true;
#endif
}

void
ICStubCompiler::leaveStubFrame(MacroAssembler& masm, bool calledIntoIon)
{
    MOZ_ASSERT(entersStubFrame_ && inStubFrame_);
    inStubFrame_ = false;
    EmitLeaveStubFrame(masm, calledIntoIon);
}

void
ICStubCompiler::pushFramePtr(MacroAssembler& masm, Register scratch)
{
    if (inStubFrame_) {
        masm.loadPtr(Address(BaselineFrameReg, 0), scratch);
        masm.pushBaselineFramePtr(scratch, scratch);
    } else {
        masm.pushBaselineFramePtr(BaselineFrameReg, scratch);
    }
}

bool
ICStubCompiler::emitPostWriteBarrierSlot(MacroAssembler& masm, Register obj, ValueOperand val,
                                         Register scratch, LiveGeneralRegisterSet saveRegs)
{
    Label skipBarrier;
    masm.branchPtrInNurseryRange(Assembler::Equal, obj, scratch, &skipBarrier);
    masm.branchValueIsNurseryObject(Assembler::NotEqual, val, scratch, &skipBarrier);

    // void PostWriteBarrier(JSRuntime* rt, JSObject* obj);
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    saveRegs.add(ICTailCallReg);
#endif
    saveRegs.set() = GeneralRegisterSet::Intersect(saveRegs.set(), GeneralRegisterSet::Volatile());
    masm.PushRegsInMask(saveRegs);
    masm.setupUnalignedABICall(2, scratch);
    masm.movePtr(ImmPtr(cx->runtime()), scratch);
    masm.passABIArg(scratch);
    masm.passABIArg(obj);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, PostWriteBarrier));
    masm.PopRegsInMask(saveRegs);

    masm.bind(&skipBarrier);
    return true;
}

} // namespace jit
} // namespace js
