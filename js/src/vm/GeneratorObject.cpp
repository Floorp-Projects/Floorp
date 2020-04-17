/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/GeneratorObject.h"

#include "js/PropertySpec.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/GlobalObject.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"  // js::PlainObject

#include "debugger/DebugAPI-inl.h"
#include "vm/ArrayObject-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Stack-inl.h"

using namespace js;

JSObject* AbstractGeneratorObject::create(JSContext* cx,
                                          AbstractFramePtr frame) {
  MOZ_ASSERT(frame.isGeneratorFrame());
  MOZ_ASSERT(frame.script()->nfixed() == 0);
  MOZ_ASSERT(!frame.isConstructing());

  RootedFunction fun(cx, frame.callee());

  Rooted<AbstractGeneratorObject*> genObj(cx);
  if (!fun->isAsync()) {
    genObj = GeneratorObject::create(cx, fun);
  } else if (fun->isGenerator()) {
    genObj = AsyncGeneratorObject::create(cx, fun);
  } else {
    genObj = AsyncFunctionGeneratorObject::create(cx, fun);
  }
  if (!genObj) {
    return nullptr;
  }

  genObj->setCallee(*frame.callee());
  genObj->setEnvironmentChain(*frame.environmentChain());
  if (frame.script()->needsArgsObj()) {
    genObj->setArgsObj(frame.argsObj());
  }
  genObj->clearExpressionStack();

  if (!DebugAPI::onNewGenerator(cx, frame, genObj)) {
    return nullptr;
  }

  return genObj;
}

void AbstractGeneratorObject::trace(JSTracer* trc) {
  DebugAPI::traceGeneratorFrame(trc, this);
}

bool AbstractGeneratorObject::suspend(JSContext* cx, HandleObject obj,
                                      AbstractFramePtr frame, jsbytecode* pc,
                                      Value* vp, unsigned nvalues) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::InitialYield || JSOp(*pc) == JSOp::Yield ||
             JSOp(*pc) == JSOp::Await);

  auto genObj = obj.as<AbstractGeneratorObject>();
  MOZ_ASSERT(!genObj->hasExpressionStack() || genObj->isExpressionStackEmpty());
  MOZ_ASSERT_IF(JSOp(*pc) == JSOp::Await, genObj->callee().isAsync());
  MOZ_ASSERT_IF(JSOp(*pc) == JSOp::Yield, genObj->callee().isGenerator());

  ArrayObject* stack = nullptr;
  if (nvalues > 0) {
    do {
      if (genObj->hasExpressionStack()) {
        MOZ_ASSERT(genObj->expressionStack().getDenseInitializedLength() == 0);
        auto result = genObj->expressionStack().setOrExtendDenseElements(
            cx, 0, vp, nvalues, ShouldUpdateTypes::DontUpdate);
        if (result == DenseElementResult::Success) {
          MOZ_ASSERT(genObj->expressionStack().getDenseInitializedLength() ==
                     nvalues);
          break;
        }
        if (result == DenseElementResult::Failure) {
          return false;
        }
      }

      stack = NewDenseCopiedArray(cx, nvalues, vp);
      if (!stack) {
        return false;
      }
    } while (false);
  }

  genObj->setResumeIndex(pc);
  genObj->setEnvironmentChain(*frame.environmentChain());
  if (stack) {
    genObj->setExpressionStack(*stack);
  }

  return true;
}

void AbstractGeneratorObject::finalSuspend(HandleObject obj) {
  auto* genObj = &obj->as<AbstractGeneratorObject>();
  MOZ_ASSERT(genObj->isRunning());
  genObj->setClosed();
}

AbstractGeneratorObject* js::GetGeneratorObjectForFrame(
    JSContext* cx, AbstractFramePtr frame) {
  cx->check(frame);
  MOZ_ASSERT(frame.isGeneratorFrame());

  if (!frame.hasInitialEnvironment()) {
    return nullptr;
  }

  // The ".generator" binding is always present and always "aliased".
  CallObject& callObj = frame.callObj();
  Shape* shape = callObj.lookup(cx, cx->names().dotGenerator);
  Value genValue = callObj.getSlot(shape->slot());

  // If the `generator; setaliasedvar ".generator"; initialyield` bytecode
  // sequence has not run yet, genValue is undefined.
  return genValue.isObject()
             ? &genValue.toObject().as<AbstractGeneratorObject>()
             : nullptr;
}

void js::SetGeneratorClosed(JSContext* cx, AbstractFramePtr frame) {
  CallObject& callObj = frame.callObj();

  // Get the generator object stored on the scope chain and close it.
  Shape* shape = callObj.lookup(cx, cx->names().dotGenerator);
  auto& genObj =
      callObj.getSlot(shape->slot()).toObject().as<AbstractGeneratorObject>();
  genObj.setClosed();
}

bool js::GeneratorThrowOrReturn(JSContext* cx, AbstractFramePtr frame,
                                Handle<AbstractGeneratorObject*> genObj,
                                HandleValue arg,
                                GeneratorResumeKind resumeKind) {
  MOZ_ASSERT(genObj->isRunning());
  if (resumeKind == GeneratorResumeKind::Throw) {
    cx->setPendingExceptionAndCaptureStack(arg);
  } else {
    MOZ_ASSERT(resumeKind == GeneratorResumeKind::Return);

    MOZ_ASSERT_IF(genObj->is<GeneratorObject>(), arg.isObject());
    frame.setReturnValue(arg);

    RootedValue closing(cx, MagicValue(JS_GENERATOR_CLOSING));
    cx->setPendingException(closing, nullptr);
  }
  return false;
}

bool AbstractGeneratorObject::resume(JSContext* cx,
                                     InterpreterActivation& activation,
                                     Handle<AbstractGeneratorObject*> genObj,
                                     HandleValue arg, HandleValue resumeKind) {
  MOZ_ASSERT(genObj->isSuspended());

  RootedFunction callee(cx, &genObj->callee());
  RootedObject envChain(cx, &genObj->environmentChain());
  if (!activation.resumeGeneratorFrame(callee, envChain)) {
    return false;
  }
  activation.regs().fp()->setResumedGenerator();

  if (genObj->hasArgsObj()) {
    activation.regs().fp()->initArgsObj(genObj->argsObj());
  }

  if (genObj->hasExpressionStack() && !genObj->isExpressionStackEmpty()) {
    uint32_t len = genObj->expressionStack().getDenseInitializedLength();
    MOZ_ASSERT(activation.regs().spForStackDepth(len));
    const Value* src = genObj->expressionStack().getDenseElements();
    mozilla::PodCopy(activation.regs().sp, src, len);
    activation.regs().sp += len;
    genObj->expressionStack().setDenseInitializedLength(0);
  }

  JSScript* script = callee->nonLazyScript();
  uint32_t offset = script->resumeOffsets()[genObj->resumeIndex()];
  activation.regs().pc = script->offsetToPC(offset);

  // Push arg, generator, resumeKind Values on the generator's stack.
  activation.regs().sp += 3;
  MOZ_ASSERT(activation.regs().spForStackDepth(activation.regs().stackDepth()));
  activation.regs().sp[-3] = arg;
  activation.regs().sp[-2] = ObjectValue(*genObj);
  activation.regs().sp[-1] = resumeKind;

  genObj->setRunning();
  return true;
}

GeneratorObject* GeneratorObject::create(JSContext* cx, HandleFunction fun) {
  MOZ_ASSERT(fun->isGenerator() && !fun->isAsync());

  // FIXME: This would be faster if we could avoid doing a lookup to get
  // the prototype for the instance.  Bug 906600.
  RootedValue pval(cx);
  if (!GetProperty(cx, fun, fun, cx->names().prototype, &pval)) {
    return nullptr;
  }
  RootedObject proto(cx, pval.isObject() ? &pval.toObject() : nullptr);
  if (!proto) {
    proto = GlobalObject::getOrCreateGeneratorObjectPrototype(cx, cx->global());
    if (!proto) {
      return nullptr;
    }
  }
  return NewObjectWithGivenProto<GeneratorObject>(cx, proto);
}

const JSClass GeneratorObject::class_ = {
    "Generator",
    JSCLASS_HAS_RESERVED_SLOTS(GeneratorObject::RESERVED_SLOTS),
    &classOps_,
};

const JSClassOps GeneratorObject::classOps_ = {
    nullptr,                                   // addProperty
    nullptr,                                   // delProperty
    nullptr,                                   // enumerate
    nullptr,                                   // newEnumerate
    nullptr,                                   // resolve
    nullptr,                                   // mayResolve
    nullptr,                                   // finalize
    nullptr,                                   // call
    nullptr,                                   // hasInstance
    nullptr,                                   // construct
    CallTraceMethod<AbstractGeneratorObject>,  // trace
};

static const JSFunctionSpec generator_methods[] = {
    JS_SELF_HOSTED_FN("next", "GeneratorNext", 1, 0),
    JS_SELF_HOSTED_FN("throw", "GeneratorThrow", 1, 0),
    JS_SELF_HOSTED_FN("return", "GeneratorReturn", 1, 0), JS_FS_END};

JSObject* js::NewSingletonObjectWithFunctionPrototype(
    JSContext* cx, Handle<GlobalObject*> global) {
  RootedObject proto(cx,
                     GlobalObject::getOrCreateFunctionPrototype(cx, global));
  if (!proto) {
    return nullptr;
  }
  RootedObject obj(cx,
                   NewSingletonObjectWithGivenProto<PlainObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }
  if (!JSObject::setDelegate(cx, obj)) {
    return nullptr;
  }
  return obj;
}

static JSObject* CreateGeneratorFunction(JSContext* cx, JSProtoKey key) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateFunctionConstructor(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  HandlePropertyName name = cx->names().GeneratorFunction;
  return NewFunctionWithProto(cx, Generator, 1, FunctionFlags::NATIVE_CTOR,
                              nullptr, name, proto, gc::AllocKind::FUNCTION,
                              SingletonObject);
}

static JSObject* CreateGeneratorFunctionPrototype(JSContext* cx,
                                                  JSProtoKey key) {
  return NewSingletonObjectWithFunctionPrototype(cx, cx->global());
}

static bool GeneratorFunctionClassFinish(JSContext* cx,
                                         HandleObject genFunction,
                                         HandleObject genFunctionProto) {
  Handle<GlobalObject*> global = cx->global();

  // Change the "constructor" property to non-writable before adding any other
  // properties, so it's still the last property and can be modified without a
  // dictionary-mode transition.
  MOZ_ASSERT(StringEqualsAscii(
      JSID_TO_LINEAR_STRING(
          genFunctionProto->as<NativeObject>().lastProperty()->propid()),
      "constructor"));
  MOZ_ASSERT(!genFunctionProto->as<NativeObject>().inDictionaryMode());

  RootedValue genFunctionVal(cx, ObjectValue(*genFunction));
  if (!DefineDataProperty(cx, genFunctionProto, cx->names().constructor,
                          genFunctionVal, JSPROP_READONLY)) {
    return false;
  }
  MOZ_ASSERT(!genFunctionProto->as<NativeObject>().inDictionaryMode());

  RootedObject iteratorProto(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!iteratorProto) {
    return false;
  }

  RootedObject genObjectProto(cx, GlobalObject::createBlankPrototypeInheriting(
                                      cx, &PlainObject::class_, iteratorProto));
  if (!genObjectProto) {
    return false;
  }
  if (!DefinePropertiesAndFunctions(cx, genObjectProto, nullptr,
                                    generator_methods) ||
      !DefineToStringTag(cx, genObjectProto, cx->names().Generator)) {
    return false;
  }

  if (!LinkConstructorAndPrototype(cx, genFunctionProto, genObjectProto,
                                   JSPROP_READONLY, JSPROP_READONLY) ||
      !DefineToStringTag(cx, genFunctionProto, cx->names().GeneratorFunction)) {
    return false;
  }

  global->setGeneratorObjectPrototype(genObjectProto);

  return true;
}

static const ClassSpec GeneratorFunctionClassSpec = {
    CreateGeneratorFunction,
    CreateGeneratorFunctionPrototype,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    GeneratorFunctionClassFinish,
    ClassSpec::DontDefineConstructor};

const JSClass js::GeneratorFunctionClass = {
    "GeneratorFunction", 0, JS_NULL_CLASS_OPS, &GeneratorFunctionClassSpec};

bool AbstractGeneratorObject::isAfterYield() {
  return isAfterYieldOrAwait(JSOp::Yield);
}

bool AbstractGeneratorObject::isAfterAwait() {
  return isAfterYieldOrAwait(JSOp::Await);
}

bool AbstractGeneratorObject::isAfterYieldOrAwait(JSOp op) {
  if (isClosed() || isRunning()) {
    return false;
  }

  JSScript* script = callee().nonLazyScript();
  jsbytecode* code = script->code();
  uint32_t nextOffset = script->resumeOffsets()[resumeIndex()];
  if (JSOp(code[nextOffset]) != JSOp::AfterYield) {
    return false;
  }

  static_assert(JSOpLength_Yield == JSOpLength_InitialYield,
                "JSOp::Yield and JSOp::InitialYield must have the same length");
  static_assert(JSOpLength_Yield == JSOpLength_Await,
                "JSOp::Yield and JSOp::Await must have the same length");

  uint32_t offset = nextOffset - JSOpLength_Yield;
  JSOp prevOp = JSOp(code[offset]);
  MOZ_ASSERT(prevOp == JSOp::InitialYield || prevOp == JSOp::Yield ||
             prevOp == JSOp::Await);

  return prevOp == op;
}

template <>
bool JSObject::is<js::AbstractGeneratorObject>() const {
  return is<GeneratorObject>() || is<AsyncFunctionGeneratorObject>() ||
         is<AsyncGeneratorObject>();
}

GeneratorResumeKind js::AtomToResumeKind(JSContext* cx, JSAtom* atom) {
  if (atom == cx->names().next) {
    return GeneratorResumeKind::Next;
  }
  if (atom == cx->names().throw_) {
    return GeneratorResumeKind::Throw;
  }
  MOZ_ASSERT(atom == cx->names().return_);
  return GeneratorResumeKind::Return;
}

JSAtom* js::ResumeKindToAtom(JSContext* cx, GeneratorResumeKind kind) {
  switch (kind) {
    case GeneratorResumeKind::Next:
      return cx->names().next;

    case GeneratorResumeKind::Throw:
      return cx->names().throw_;

    case GeneratorResumeKind::Return:
      return cx->names().return_;
  }
  MOZ_CRASH("Invalid resume kind");
}
