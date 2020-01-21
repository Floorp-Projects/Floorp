/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=0 ft=c:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ObjLiteral.h"
#include "mozilla/DebugOnly.h"
#include "js/RootingAPI.h"
#include "vm/JSAtom.h"
#include "vm/JSObject.h"
#include "vm/ObjectGroup.h"

#include "gc/ObjectKind-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"

namespace js {

static void InterpretObjLiteralValue(ObjLiteralAtomVector& atoms,
                                     const ObjLiteralInsn& insn,
                                     MutableHandleValue propVal) {
  switch (insn.getOp()) {
    case ObjLiteralOpcode::ConstValue:
      propVal.set(insn.getConstValue());
      break;
    case ObjLiteralOpcode::ConstAtom: {
      uint32_t index = insn.getAtomIndex();
      propVal.setString(atoms[index]);
      break;
    }
    case ObjLiteralOpcode::Null:
      propVal.setNull();
      break;
    case ObjLiteralOpcode::Undefined:
      propVal.setUndefined();
      break;
    case ObjLiteralOpcode::True:
      propVal.setBoolean(true);
      break;
    case ObjLiteralOpcode::False:
      propVal.setBoolean(false);
      break;
    default:
      MOZ_CRASH("Unexpected object-literal instruction opcode");
  }
}

static JSObject* InterpretObjLiteralObj(
    JSContext* cx, ObjLiteralAtomVector& atoms,
    mozilla::Span<const uint8_t> literalInsns, ObjLiteralFlags flags) {
  bool specificGroup = flags.contains(ObjLiteralFlag::SpecificGroup);
  bool singleton = flags.contains(ObjLiteralFlag::Singleton);
  bool noValues = flags.contains(ObjLiteralFlag::NoValues);

  ObjLiteralReader reader(literalInsns);
  ObjLiteralInsn insn;

  jsid propId;
  Rooted<Value> propVal(cx);
  Rooted<IdValueVector> properties(cx, IdValueVector(cx));

  // Compute property values and build the key/value-pair list.
  while (true) {
    if (!reader.readInsn(&insn)) {
      break;
    }
    MOZ_ASSERT(insn.isValid());

    if (insn.getKey().isArrayIndex()) {
      propId = INT_TO_JSID(insn.getKey().getArrayIndex());
    } else {
      propId = AtomToId(atoms[insn.getKey().getAtomIndex()]);
    }

    if (noValues) {
      propVal.setUndefined();
    } else {
      InterpretObjLiteralValue(atoms, insn, &propVal);
    }

    if (!properties.append(IdValuePair(propId, propVal))) {
      return nullptr;
    }
  }

  if (specificGroup) {
    return ObjectGroup::newPlainObject(
        cx, properties.begin(), properties.length(),
        singleton ? SingletonObject : TenuredObject);
  }

  gc::AllocKind allocKind = gc::GetGCObjectKind(properties.length());
  RootedPlainObject result(
      cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind, TenuredObject));
  if (!result) {
    return nullptr;
  }

  Rooted<JS::PropertyKey> propKey(cx);
  for (const auto& kvPair : properties) {
    propKey.set(kvPair.id);
    propVal.set(kvPair.value);
    if (!NativeDefineDataProperty(cx, result, propKey, propVal,
                                  JSPROP_ENUMERATE)) {
      return nullptr;
    }
  }
  return result;
}

static JSObject* InterpretObjLiteralArray(
    JSContext* cx, ObjLiteralAtomVector& atoms,
    mozilla::Span<const uint8_t> literalInsns, ObjLiteralFlags flags) {
  bool isCow = flags.contains(ObjLiteralFlag::ArrayCOW);
  ObjLiteralReader reader(literalInsns);
  ObjLiteralInsn insn;

  Rooted<ValueVector> elements(cx, ValueVector(cx));
  Rooted<Value> propVal(cx);

  while (true) {
    if (!reader.readInsn(&insn)) {
      break;
    }
    MOZ_ASSERT(insn.isValid());

    propVal.setUndefined();
    InterpretObjLiteralValue(atoms, insn, &propVal);
    if (!elements.append(propVal)) {
      return nullptr;
    }
  }

  ObjectGroup::NewArrayKind arrayKind =
      isCow ? ObjectGroup::NewArrayKind::CopyOnWrite
            : ObjectGroup::NewArrayKind::Normal;
  RootedObject result(
      cx, ObjectGroup::newArrayObject(cx, elements.begin(), elements.length(),
                                      NewObjectKind::TenuredObject, arrayKind));
  if (!result) {
    return nullptr;
  }

  return result;
}

JSObject* InterpretObjLiteral(JSContext* cx, ObjLiteralAtomVector& atoms,
                              mozilla::Span<const uint8_t> literalInsns,
                              ObjLiteralFlags flags) {
  return flags.contains(ObjLiteralFlag::Array)
             ? InterpretObjLiteralArray(cx, atoms, literalInsns, flags)
             : InterpretObjLiteralObj(cx, atoms, literalInsns, flags);
}

}  // namespace js
