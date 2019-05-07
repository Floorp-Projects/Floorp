/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/GeneratorObject.h"

#include "js/PropertySpec.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/JSObject.h"

#include "vm/ArrayObject-inl.h"
#include "vm/Debugger-inl.h"
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

  if (!Debugger::onNewGenerator(cx, frame, genObj)) {
    return nullptr;
  }

  return genObj;
}

bool AbstractGeneratorObject::suspend(JSContext* cx, HandleObject obj,
                                      AbstractFramePtr frame, jsbytecode* pc,
                                      Value* vp, unsigned nvalues) {
  MOZ_ASSERT(*pc == JSOP_INITIALYIELD || *pc == JSOP_YIELD ||
             *pc == JSOP_AWAIT);

  auto genObj = obj.as<AbstractGeneratorObject>();
  MOZ_ASSERT(!genObj->hasExpressionStack() || genObj->isExpressionStackEmpty());
  MOZ_ASSERT_IF(*pc == JSOP_AWAIT, genObj->callee().isAsync());
  MOZ_ASSERT_IF(*pc == JSOP_YIELD, genObj->callee().isGenerator());

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
                                     HandleValue arg) {
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

  // Always push on a value, even if we are raising an exception. In the
  // exception case, the stack needs to have something on it so that exception
  // handling doesn't skip the catch blocks. See TryNoteIter::settle.
  activation.regs().sp++;
  MOZ_ASSERT(activation.regs().spForStackDepth(activation.regs().stackDepth()));
  activation.regs().sp[-1] = arg;

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

const Class GeneratorObject::class_ = {
    "Generator", JSCLASS_HAS_RESERVED_SLOTS(GeneratorObject::RESERVED_SLOTS)};

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
  RootedObject obj(
      cx, NewObjectWithGivenProto<PlainObject>(cx, proto, SingletonObject));
  if (!obj) {
    return nullptr;
  }
  if (!JSObject::setDelegate(cx, obj)) {
    return nullptr;
  }
  return obj;
}

/* static */
bool GlobalObject::initGenerators(JSContext* cx, Handle<GlobalObject*> global) {
  if (global->getReservedSlot(GENERATOR_OBJECT_PROTO).isObject()) {
    return true;
  }

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

  RootedObject genFunctionProto(
      cx, NewSingletonObjectWithFunctionPrototype(cx, global));
  if (!genFunctionProto) {
    return false;
  }
  if (!LinkConstructorAndPrototype(cx, genFunctionProto, genObjectProto,
                                   JSPROP_READONLY, JSPROP_READONLY) ||
      !DefineToStringTag(cx, genFunctionProto, cx->names().GeneratorFunction)) {
    return false;
  }

  RootedObject proto(
      cx, GlobalObject::getOrCreateFunctionConstructor(cx, cx->global()));
  if (!proto) {
    return false;
  }
  HandlePropertyName name = cx->names().GeneratorFunction;
  RootedObject genFunction(
      cx, NewFunctionWithProto(cx, Generator, 1, JSFunction::NATIVE_CTOR,
                               nullptr, name, proto, gc::AllocKind::FUNCTION,
                               SingletonObject));
  if (!genFunction) {
    return false;
  }
  if (!LinkConstructorAndPrototype(cx, genFunction, genFunctionProto,
                                   JSPROP_PERMANENT | JSPROP_READONLY,
                                   JSPROP_READONLY)) {
    return false;
  }

  global->setReservedSlot(GENERATOR_OBJECT_PROTO, ObjectValue(*genObjectProto));
  global->setReservedSlot(GENERATOR_FUNCTION, ObjectValue(*genFunction));
  global->setReservedSlot(GENERATOR_FUNCTION_PROTO,
                          ObjectValue(*genFunctionProto));
  return true;
}

bool AbstractGeneratorObject::isAfterYield() {
  return isAfterYieldOrAwait(JSOP_YIELD);
}

bool AbstractGeneratorObject::isAfterAwait() {
  return isAfterYieldOrAwait(JSOP_AWAIT);
}

bool AbstractGeneratorObject::isAfterYieldOrAwait(JSOp op) {
  if (isClosed() || isRunning()) {
    return false;
  }

  JSScript* script = callee().nonLazyScript();
  jsbytecode* code = script->code();
  uint32_t nextOffset = script->resumeOffsets()[resumeIndex()];
  if (code[nextOffset] != JSOP_AFTERYIELD) {
    return false;
  }

  static_assert(JSOP_YIELD_LENGTH == JSOP_INITIALYIELD_LENGTH,
                "JSOP_YIELD and JSOP_INITIALYIELD must have the same length");
  static_assert(JSOP_YIELD_LENGTH == JSOP_AWAIT_LENGTH,
                "JSOP_YIELD and JSOP_AWAIT must have the same length");

  uint32_t offset = nextOffset - JSOP_YIELD_LENGTH;
  MOZ_ASSERT(code[offset] == JSOP_INITIALYIELD || code[offset] == JSOP_YIELD ||
             code[offset] == JSOP_AWAIT);

  return code[offset] == op;
}

template <>
bool JSObject::is<js::AbstractGeneratorObject>() const {
  return is<GeneratorObject>() || is<AsyncFunctionGeneratorObject>() ||
         is<AsyncGeneratorObject>();
}
