/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Interpreter_inl_h
#define vm_Interpreter_inl_h

#include "vm/Interpreter.h"

#include "jsnum.h"

#include "jit/Ion.h"
#include "vm/ArgumentsObject.h"
#include "vm/BytecodeUtil.h"  // JSDVG_SEARCH_STACK
#include "vm/Realm.h"

#include "vm/EnvironmentObject-inl.h"
#include "vm/GlobalObject-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/ObjectOperations-inl.h"
#include "vm/Stack-inl.h"
#include "vm/StringType-inl.h"

namespace js {

/*
 * Every possible consumer of MagicValue(JS_OPTIMIZED_ARGUMENTS) (as determined
 * by ScriptAnalysis::needsArgsObj) must check for these magic values and, when
 * one is received, act as if the value were the function's ArgumentsObject.
 * Additionally, it is possible that, after 'arguments' was copied into a
 * temporary, the arguments object has been created a some other failed guard
 * that called JSScript::argumentsOptimizationFailed. In this case, it is
 * always valid (and necessary) to replace JS_OPTIMIZED_ARGUMENTS with the real
 * arguments object.
 */
static inline bool IsOptimizedArguments(AbstractFramePtr frame,
                                        MutableHandleValue vp) {
  if (vp.isMagic(JS_OPTIMIZED_ARGUMENTS) && frame.script()->needsArgsObj()) {
    vp.setObject(frame.argsObj());
  }
  return vp.isMagic(JS_OPTIMIZED_ARGUMENTS);
}

/*
 * One optimized consumer of MagicValue(JS_OPTIMIZED_ARGUMENTS) is f.apply.
 * However, this speculation must be guarded before calling 'apply' in case it
 * is not the builtin Function.prototype.apply.
 */
static inline void GuardFunApplyArgumentsOptimization(JSContext* cx,
                                                      AbstractFramePtr frame,
                                                      CallArgs& args) {
  if (args.length() == 2 && IsOptimizedArguments(frame, args[1])) {
    if (!IsNativeFunction(args.calleev(), js::fun_apply)) {
      RootedScript script(cx, frame.script());
      JSScript::argumentsOptimizationFailed(cx, script);
      args[1].setObject(frame.argsObj());
    }
  }
}

/*
 * Per ES6, lexical declarations may not be accessed in any fashion until they
 * are initialized (i.e., until the actual declaring statement is
 * executed). The various LEXICAL opcodes need to check if the slot is an
 * uninitialized let declaration, represented by the magic value
 * JS_UNINITIALIZED_LEXICAL.
 */
static inline bool IsUninitializedLexical(const Value& val) {
  // Use whyMagic here because JS_OPTIMIZED_ARGUMENTS could flow into here.
  return val.isMagic() && val.whyMagic() == JS_UNINITIALIZED_LEXICAL;
}

static inline bool IsUninitializedLexicalSlot(HandleObject obj,
                                              Handle<PropertyResult> prop) {
  MOZ_ASSERT(prop);
  if (obj->is<WithEnvironmentObject>()) {
    return false;
  }

  // Proxy hooks may return a non-native property.
  if (prop.isNonNativeProperty()) {
    return false;
  }

  Shape* shape = prop.shape();
  if (!shape->isDataProperty()) {
    return false;
  }

  MOZ_ASSERT(obj->as<NativeObject>().containsPure(shape));
  return IsUninitializedLexical(obj->as<NativeObject>().getSlot(shape->slot()));
}

static inline bool CheckUninitializedLexical(JSContext* cx, PropertyName* name_,
                                             HandleValue val) {
  if (IsUninitializedLexical(val)) {
    RootedPropertyName name(cx, name_);
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, name);
    return false;
  }
  return true;
}

inline bool GetLengthProperty(const Value& lval, MutableHandleValue vp) {
  /* Optimize length accesses on strings, arrays, and arguments. */
  if (lval.isString()) {
    vp.setInt32(lval.toString()->length());
    return true;
  }
  if (lval.isObject()) {
    JSObject* obj = &lval.toObject();
    if (obj->is<ArrayObject>()) {
      vp.setNumber(obj->as<ArrayObject>().length());
      return true;
    }

    if (obj->is<ArgumentsObject>()) {
      ArgumentsObject* argsobj = &obj->as<ArgumentsObject>();
      if (!argsobj->hasOverriddenLength()) {
        uint32_t length = argsobj->initialLength();
        MOZ_ASSERT(length < INT32_MAX);
        vp.setInt32(int32_t(length));
        return true;
      }
    }
  }

  return false;
}

enum class GetNameMode { Normal, TypeOf };

template <GetNameMode mode>
inline bool FetchName(JSContext* cx, HandleObject receiver, HandleObject holder,
                      HandlePropertyName name, Handle<PropertyResult> prop,
                      MutableHandleValue vp) {
  if (!prop) {
    switch (mode) {
      case GetNameMode::Normal:
        ReportIsNotDefined(cx, name);
        return false;
      case GetNameMode::TypeOf:
        vp.setUndefined();
        return true;
    }
  }

  /* Take the slow path if shape was not found in a native object. */
  if (!receiver->isNative() || !holder->isNative()) {
    Rooted<jsid> id(cx, NameToId(name));
    if (!GetProperty(cx, receiver, receiver, id, vp)) {
      return false;
    }
  } else {
    RootedShape shape(cx, prop.shape());
    if (shape->isDataDescriptor() && shape->hasDefaultGetter()) {
      /* Fast path for Object instance properties. */
      MOZ_ASSERT(shape->isDataProperty());
      vp.set(holder->as<NativeObject>().getSlot(shape->slot()));
    } else {
      // Unwrap 'with' environments for reasons given in
      // GetNameBoundInEnvironment.
      RootedObject normalized(cx, MaybeUnwrapWithEnvironment(receiver));
      if (!NativeGetExistingProperty(cx, normalized, holder.as<NativeObject>(),
                                     shape, vp)) {
        return false;
      }
    }
  }

  // We do our own explicit checking for |this|
  if (name == cx->names().dotThis) {
    return true;
  }

  // NAME operations are the slow paths already, so unconditionally check
  // for uninitialized lets.
  return CheckUninitializedLexical(cx, name, vp);
}

inline bool FetchNameNoGC(JSObject* pobj, PropertyResult prop, Value* vp) {
  if (!prop || !pobj->isNative()) {
    return false;
  }

  Shape* shape = prop.shape();
  if (!shape->isDataDescriptor() || !shape->hasDefaultGetter()) {
    return false;
  }

  *vp = pobj->as<NativeObject>().getSlot(shape->slot());
  return !IsUninitializedLexical(*vp);
}

template <js::GetNameMode mode>
inline bool GetEnvironmentName(JSContext* cx, HandleObject envChain,
                               HandlePropertyName name, MutableHandleValue vp) {
  {
    PropertyResult prop;
    JSObject* obj = nullptr;
    JSObject* pobj = nullptr;
    if (LookupNameNoGC(cx, name, envChain, &obj, &pobj, &prop)) {
      if (FetchNameNoGC(pobj, prop, vp.address())) {
        return true;
      }
    }
  }

  Rooted<PropertyResult> prop(cx);
  RootedObject obj(cx), pobj(cx);
  if (!LookupName(cx, name, envChain, &obj, &pobj, &prop)) {
    return false;
  }

  return FetchName<mode>(cx, obj, pobj, name, prop, vp);
}

inline bool HasOwnProperty(JSContext* cx, HandleValue val, HandleValue idValue,
                           bool* result) {
  // As an optimization, provide a fast path when rooting is not necessary and
  // we can safely retrieve the object's shape.
  jsid id;
  if (val.isObject() && ValueToId<NoGC>(cx, idValue, &id)) {
    JSObject* obj = &val.toObject();
    PropertyResult prop;
    if (obj->isNative() && NativeLookupOwnProperty<NoGC>(
                               cx, &obj->as<NativeObject>(), id, &prop)) {
      *result = prop.isFound();
      return true;
    }
  }

  // Step 1.
  RootedId key(cx);
  if (!ToPropertyKey(cx, idValue, &key)) {
    return false;
  }

  // Step 2.
  RootedObject obj(cx, ToObject(cx, val));
  if (!obj) {
    return false;
  }

  // Step 3.
  return HasOwnProperty(cx, obj, key, result);
}

inline bool GetIntrinsicOperation(JSContext* cx, HandleScript script,
                                  jsbytecode* pc, MutableHandleValue vp) {
  RootedPropertyName name(cx, script->getName(pc));
  return GlobalObject::getIntrinsicValue(cx, cx->global(), name, vp);
}

inline bool SetIntrinsicOperation(JSContext* cx, JSScript* script,
                                  jsbytecode* pc, HandleValue val) {
  RootedPropertyName name(cx, script->getName(pc));
  return GlobalObject::setIntrinsicValue(cx, cx->global(), name, val);
}

inline void SetAliasedVarOperation(JSContext* cx, JSScript* script,
                                   jsbytecode* pc, EnvironmentObject& obj,
                                   EnvironmentCoordinate ec, const Value& val,
                                   MaybeCheckTDZ checkTDZ) {
  MOZ_ASSERT_IF(checkTDZ, !IsUninitializedLexical(obj.aliasedBinding(ec)));
  obj.setAliasedBinding(cx, ec, val);
}

inline bool SetNameOperation(JSContext* cx, JSScript* script, jsbytecode* pc,
                             HandleObject env, HandleValue val) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::SetName || JSOp(*pc) == JSOp::StrictSetName ||
             JSOp(*pc) == JSOp::SetGName || JSOp(*pc) == JSOp::StrictSetGName);
  MOZ_ASSERT_IF(
      (JSOp(*pc) == JSOp::SetGName || JSOp(*pc) == JSOp::StrictSetGName) &&
          !script->hasNonSyntacticScope(),
      env == cx->global() || env == &cx->global()->lexicalEnvironment() ||
          env->is<RuntimeLexicalErrorObject>());

  bool strict =
      JSOp(*pc) == JSOp::StrictSetName || JSOp(*pc) == JSOp::StrictSetGName;
  RootedPropertyName name(cx, script->getName(pc));

  // In strict mode, assigning to an undeclared global variable is an
  // error. To detect this, we call NativeSetProperty directly and pass
  // Unqualified. It stores the error, if any, in |result|.
  bool ok;
  ObjectOpResult result;
  RootedId id(cx, NameToId(name));
  RootedValue receiver(cx, ObjectValue(*env));
  if (env->isUnqualifiedVarObj()) {
    RootedNativeObject varobj(cx);
    if (env->is<DebugEnvironmentProxy>()) {
      varobj =
          &env->as<DebugEnvironmentProxy>().environment().as<NativeObject>();
    } else {
      varobj = &env->as<NativeObject>();
    }
    MOZ_ASSERT(!varobj->getOpsSetProperty());
    ok = NativeSetProperty<Unqualified>(cx, varobj, id, val, receiver, result);
  } else {
    ok = SetProperty(cx, env, id, val, receiver, result);
  }
  return ok && result.checkStrictModeError(cx, env, id, strict);
}

inline void InitGlobalLexicalOperation(JSContext* cx,
                                       LexicalEnvironmentObject* lexicalEnvArg,
                                       JSScript* script, jsbytecode* pc,
                                       HandleValue value) {
  MOZ_ASSERT_IF(!script->hasNonSyntacticScope(),
                lexicalEnvArg == &cx->global()->lexicalEnvironment());
  MOZ_ASSERT(JSOp(*pc) == JSOp::InitGLexical);
  Rooted<LexicalEnvironmentObject*> lexicalEnv(cx, lexicalEnvArg);
  RootedShape shape(cx, lexicalEnv->lookup(cx, script->getName(pc)));
  MOZ_ASSERT(shape);
  MOZ_ASSERT(IsUninitializedLexical(lexicalEnv->getSlot(shape->slot())));

  // Don't treat the initial assignment to global lexicals as overwrites.
  lexicalEnv->setSlotWithType(cx, shape, value, /* overwriting = */ false);
}

inline bool InitPropertyOperation(JSContext* cx, JSOp op, HandleObject obj,
                                  HandlePropertyName name, HandleValue rhs) {
  unsigned propAttrs = GetInitDataPropAttrs(op);
  return DefineDataProperty(cx, obj, name, rhs, propAttrs);
}

static MOZ_ALWAYS_INLINE bool NegOperation(JSContext* cx,
                                           MutableHandleValue val,
                                           MutableHandleValue res) {
  /*
   * When the operand is int jsval, INT32_FITS_IN_JSVAL(i) implies
   * INT32_FITS_IN_JSVAL(-i) unless i is 0 or INT32_MIN when the
   * results, -0.0 or INT32_MAX + 1, are double values.
   */
  int32_t i;
  if (val.isInt32() && (i = val.toInt32()) != 0 && i != INT32_MIN) {
    res.setInt32(-i);
    return true;
  }

  if (!ToNumeric(cx, val)) {
    return false;
  }

  if (val.isBigInt()) {
    return BigInt::negValue(cx, val, res);
  }

  res.setNumber(-val.toNumber());
  return true;
}

static MOZ_ALWAYS_INLINE bool IncOperation(JSContext* cx, HandleValue val,
                                           MutableHandleValue res) {
  int32_t i;
  if (val.isInt32() && (i = val.toInt32()) != INT32_MAX) {
    res.setInt32(i + 1);
    return true;
  }

  if (val.isNumber()) {
    res.setNumber(val.toNumber() + 1);
    return true;
  }

  MOZ_ASSERT(val.isBigInt(), "+1 only callable on result of JSOp::ToNumeric");
  return BigInt::incValue(cx, val, res);
}

static MOZ_ALWAYS_INLINE bool DecOperation(JSContext* cx, HandleValue val,
                                           MutableHandleValue res) {
  int32_t i;
  if (val.isInt32() && (i = val.toInt32()) != INT32_MIN) {
    res.setInt32(i - 1);
    return true;
  }

  if (val.isNumber()) {
    res.setNumber(val.toNumber() - 1);
    return true;
  }

  MOZ_ASSERT(val.isBigInt(), "-1 only callable on result of JSOp::ToNumeric");
  return BigInt::decValue(cx, val, res);
}

static MOZ_ALWAYS_INLINE bool ToIdOperation(JSContext* cx, HandleValue idval,
                                            MutableHandleValue res) {
  if (idval.isInt32()) {
    res.set(idval);
    return true;
  }

  RootedId id(cx);
  if (!ToPropertyKey(cx, idval, &id)) {
    return false;
  }

  res.set(IdToValue(id));
  return true;
}

static MOZ_ALWAYS_INLINE bool GetObjectElementOperation(
    JSContext* cx, JSOp op, JS::HandleObject obj, JS::HandleValue receiver,
    HandleValue key, MutableHandleValue res) {
  MOZ_ASSERT(op == JSOp::GetElem || op == JSOp::CallElem ||
             op == JSOp::GetElemSuper);
  MOZ_ASSERT_IF(op == JSOp::GetElem || op == JSOp::CallElem,
                obj == &receiver.toObject());

  do {
    uint32_t index;
    if (IsDefinitelyIndex(key, &index)) {
      if (GetElementNoGC(cx, obj, receiver, index, res.address())) {
        break;
      }

      if (!GetElement(cx, obj, receiver, index, res)) {
        return false;
      }
      break;
    }

    if (key.isString()) {
      JSString* str = key.toString();
      JSAtom* name = str->isAtom() ? &str->asAtom() : AtomizeString(cx, str);
      if (!name) {
        return false;
      }
      if (name->isIndex(&index)) {
        if (GetElementNoGC(cx, obj, receiver, index, res.address())) {
          break;
        }
      } else {
        if (GetPropertyNoGC(cx, obj, receiver, name->asPropertyName(),
                            res.address())) {
          break;
        }
      }
    }

    RootedId id(cx);
    if (!ToPropertyKey(cx, key, &id)) {
      return false;
    }
    if (!GetProperty(cx, obj, receiver, id, res)) {
      return false;
    }
  } while (false);

  cx->debugOnlyCheck(res);
  return true;
}

static MOZ_ALWAYS_INLINE bool GetPrimitiveElementOperation(
    JSContext* cx, JSOp op, JS::HandleValue receiver, int receiverIndex,
    HandleValue key, MutableHandleValue res) {
  MOZ_ASSERT(op == JSOp::GetElem || op == JSOp::CallElem);

  // FIXME: Bug 1234324 We shouldn't be boxing here.
  RootedObject boxed(
      cx, ToObjectFromStackForPropertyAccess(cx, receiver, receiverIndex, key));
  if (!boxed) {
    return false;
  }

  do {
    uint32_t index;
    if (IsDefinitelyIndex(key, &index)) {
      if (GetElementNoGC(cx, boxed, receiver, index, res.address())) {
        break;
      }

      if (!GetElement(cx, boxed, receiver, index, res)) {
        return false;
      }
      break;
    }

    if (key.isString()) {
      JSString* str = key.toString();
      JSAtom* name = str->isAtom() ? &str->asAtom() : AtomizeString(cx, str);
      if (!name) {
        return false;
      }
      if (name->isIndex(&index)) {
        if (GetElementNoGC(cx, boxed, receiver, index, res.address())) {
          break;
        }
      } else {
        if (GetPropertyNoGC(cx, boxed, receiver, name->asPropertyName(),
                            res.address())) {
          break;
        }
      }
    }

    RootedId id(cx);
    if (!ToPropertyKey(cx, key, &id)) {
      return false;
    }
    if (!GetProperty(cx, boxed, receiver, id, res)) {
      return false;
    }
  } while (false);

  cx->debugOnlyCheck(res);
  return true;
}

static MOZ_ALWAYS_INLINE bool GetElemOptimizedArguments(
    JSContext* cx, AbstractFramePtr frame, MutableHandleValue lref,
    HandleValue rref, MutableHandleValue res, bool* done) {
  MOZ_ASSERT(!*done);

  if (IsOptimizedArguments(frame, lref)) {
    if (rref.isInt32()) {
      int32_t i = rref.toInt32();
      if (i >= 0 && uint32_t(i) < frame.numActualArgs()) {
        res.set(frame.unaliasedActual(i));
        *done = true;
        return true;
      }
    }

    RootedScript script(cx, frame.script());
    JSScript::argumentsOptimizationFailed(cx, script);

    lref.set(ObjectValue(frame.argsObj()));
  }

  return true;
}

static MOZ_ALWAYS_INLINE bool GetElementOperationWithStackIndex(
    JSContext* cx, JSOp op, HandleValue lref, int lrefIndex, HandleValue rref,
    MutableHandleValue res) {
  MOZ_ASSERT(op == JSOp::GetElem || op == JSOp::CallElem);

  uint32_t index;
  if (lref.isString() && IsDefinitelyIndex(rref, &index)) {
    JSString* str = lref.toString();
    if (index < str->length()) {
      str = cx->staticStrings().getUnitStringForElement(cx, str, index);
      if (!str) {
        return false;
      }
      res.setString(str);
      return true;
    }
  }

  if (lref.isPrimitive()) {
    RootedValue thisv(cx, lref);
    return GetPrimitiveElementOperation(cx, op, thisv, lrefIndex, rref, res);
  }

  RootedObject obj(cx, &lref.toObject());
  RootedValue thisv(cx, lref);
  return GetObjectElementOperation(cx, op, obj, thisv, rref, res);
}

// Wrapper for callVM from JIT.
static MOZ_ALWAYS_INLINE bool GetElementOperation(JSContext* cx, JSOp op,
                                                  HandleValue lref,
                                                  HandleValue rref,
                                                  MutableHandleValue res) {
  return GetElementOperationWithStackIndex(cx, op, lref, JSDVG_SEARCH_STACK,
                                           rref, res);
}

static MOZ_ALWAYS_INLINE JSString* TypeOfOperation(const Value& v,
                                                   JSRuntime* rt) {
  JSType type = js::TypeOfValue(v);
  return TypeName(type, *rt->commonNames);
}

static MOZ_ALWAYS_INLINE bool InitElemOperation(JSContext* cx, jsbytecode* pc,
                                                HandleObject obj,
                                                HandleValue idval,
                                                HandleValue val) {
  MOZ_ASSERT(!val.isMagic(JS_ELEMENTS_HOLE));

  RootedId id(cx);
  if (!ToPropertyKey(cx, idval, &id)) {
    return false;
  }

  unsigned flags = GetInitDataPropAttrs(JSOp(*pc));
  return DefineDataProperty(cx, obj, id, val, flags);
}

static MOZ_ALWAYS_INLINE bool InitArrayElemOperation(JSContext* cx,
                                                     jsbytecode* pc,
                                                     HandleObject obj,
                                                     uint32_t index,
                                                     HandleValue val) {
  JSOp op = JSOp(*pc);
  MOZ_ASSERT(op == JSOp::InitElemArray || op == JSOp::InitElemInc);

  MOZ_ASSERT(obj->is<ArrayObject>());

  // The JITs depend on InitElemArray's index not exceeding the dense element
  // capacity.
  MOZ_ASSERT_IF(op == JSOp::InitElemArray,
                index < obj->as<ArrayObject>().getDenseCapacity());

  if (op == JSOp::InitElemInc && index == INT32_MAX) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_SPREAD_TOO_LARGE);
    return false;
  }

  /*
   * If val is a hole, do not call DefineElement.
   *
   * Furthermore, if the current op is JSOp::InitElemInc, always call
   * SetLengthProperty even if it is not the last element initialiser, because
   * it may be followed by a SpreadElement loop, which will not set the array
   * length if nothing is spread.
   *
   * Alternatively, if the current op is JSOp::InitElemArray, the length will
   * have already been set by the earlier JSOp::NewArray; JSOp::InitElemArray
   * cannot follow SpreadElements.
   */
  if (val.isMagic(JS_ELEMENTS_HOLE)) {
    if (op == JSOp::InitElemInc) {
      if (!SetLengthProperty(cx, obj, index + 1)) {
        return false;
      }
    }
  } else {
    if (!DefineDataElement(cx, obj, index, val, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  return true;
}

static inline ArrayObject* ProcessCallSiteObjOperation(JSContext* cx,
                                                       HandleScript script,
                                                       jsbytecode* pc) {
  MOZ_ASSERT(JSOp(*pc) == JSOp::CallSiteObj);

  RootedArrayObject cso(cx, &script->getObject(pc)->as<ArrayObject>());

  if (cso->isExtensible()) {
    RootedObject raw(cx, script->getObject(GET_UINT32_INDEX(pc) + 1));
    MOZ_ASSERT(raw->is<ArrayObject>());

    RootedValue rawValue(cx, ObjectValue(*raw));
    if (!DefineDataProperty(cx, cso, cx->names().raw, rawValue, 0)) {
      return nullptr;
    }
    if (!FreezeObject(cx, raw)) {
      return nullptr;
    }
    if (!FreezeObject(cx, cso)) {
      return nullptr;
    }
  }

  return cso;
}

inline JSFunction* ReportIfNotFunction(
    JSContext* cx, HandleValue v, MaybeConstruct construct = NO_CONSTRUCT) {
  if (v.isObject() && v.toObject().is<JSFunction>()) {
    return &v.toObject().as<JSFunction>();
  }

  ReportIsNotFunction(cx, v, -1, construct);
  return nullptr;
}

} /* namespace js */

#endif /* vm_Interpreter_inl_h */
