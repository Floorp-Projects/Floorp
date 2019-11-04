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
  bool singleton = flags & OBJ_LITERAL_SINGLETON;

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
    propVal.setUndefined();
    InterpretObjLiteralValue(atoms, insn, &propVal);
    if (!properties.append(IdValuePair(propId, propVal))) {
      return nullptr;
    }
  }

  // Actually build the object:
  // - In the singleton case, we want to collect all properties *first*, then
  //   call ObjectGroup::newPlainObject at the end to build a group specific to
  //   this singleton object.
  // - In the non-singleton case (template case), we want to allocate an
  //   ordinary tenured object, *not* necessarily with its own group, to prevent
  //   memory regressions (too many groups compared to old behavior).
  if (singleton) {
    return ObjectGroup::newPlainObject(cx, properties.begin(),
                                       properties.length(), SingletonObject);
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
  ObjLiteralReader reader(literalInsns);
  ObjLiteralInsn insn;

  Rooted<ValueVector> elements(cx, ValueVector(cx));
  Rooted<Value> propVal(cx);

  mozilla::DebugOnly<uint32_t> index = 0;
  while (true) {
    if (!reader.readInsn(&insn)) {
      break;
    }
    MOZ_ASSERT(insn.isValid());

    MOZ_ASSERT(insn.getKey().isArrayIndex());
    MOZ_ASSERT(insn.getKey().getArrayIndex() == index);
#ifdef DEBUG
    index++;
#endif
    propVal.setUndefined();
    InterpretObjLiteralValue(atoms, insn, &propVal);
    if (!elements.append(propVal)) {
      return nullptr;
    }
  }

  ObjectGroup::NewArrayKind arrayKind =
      (flags & OBJ_LITERAL_SINGLETON) ? ObjectGroup::NewArrayKind::Normal
                                      : ObjectGroup::NewArrayKind::CopyOnWrite;
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
  return (flags & OBJ_LITERAL_ARRAY)
             ? InterpretObjLiteralArray(cx, atoms, literalInsns, flags)
             : InterpretObjLiteralObj(cx, atoms, literalInsns, flags);
}

}  // namespace js
