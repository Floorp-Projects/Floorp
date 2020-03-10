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

static JS::Value InterpretObjLiteralValue(const ObjLiteralAtomVector& atoms,
                                          const ObjLiteralInsn& insn) {
  switch (insn.getOp()) {
    case ObjLiteralOpcode::ConstValue:
      return insn.getConstValue();
    case ObjLiteralOpcode::ConstAtom: {
      uint32_t index = insn.getAtomIndex();
      return StringValue(atoms[index]);
    }
    case ObjLiteralOpcode::Null:
      return NullValue();
    case ObjLiteralOpcode::Undefined:
      return UndefinedValue();
    case ObjLiteralOpcode::True:
      return BooleanValue(true);
    case ObjLiteralOpcode::False:
      return BooleanValue(false);
    default:
      MOZ_CRASH("Unexpected object-literal instruction opcode");
  }
}

static JSObject* InterpretObjLiteralObj(
    JSContext* cx, const ObjLiteralAtomVector& atoms,
    const mozilla::Span<const uint8_t> literalInsns, ObjLiteralFlags flags) {
  bool specificGroup = flags.contains(ObjLiteralFlag::SpecificGroup);
  bool singleton = flags.contains(ObjLiteralFlag::Singleton);
  bool noValues = flags.contains(ObjLiteralFlag::NoValues);

  ObjLiteralReader reader(literalInsns);
  ObjLiteralInsn insn;

  Rooted<IdValueVector> properties(cx, IdValueVector(cx));

  // Compute property values and build the key/value-pair list.
  while (reader.readInsn(&insn)) {
    MOZ_ASSERT(insn.isValid());

    jsid propId;
    if (insn.getKey().isArrayIndex()) {
      propId = INT_TO_JSID(insn.getKey().getArrayIndex());
    } else {
      propId = AtomToId(atoms[insn.getKey().getAtomIndex()]);
    }

    JS::Value propVal;
    if (!noValues) {
      propVal = InterpretObjLiteralValue(atoms, insn);
    }

    if (!properties.emplaceBack(propId, propVal)) {
      return nullptr;
    }
  }

  if (specificGroup) {
    return ObjectGroup::newPlainObject(
        cx, properties.begin(), properties.length(),
        singleton ? SingletonObject : TenuredObject);
  }

  return NewPlainObjectWithProperties(cx, properties.begin(),
                                      properties.length(), TenuredObject);
}

static JSObject* InterpretObjLiteralArray(
    JSContext* cx, const ObjLiteralAtomVector& atoms,
    const mozilla::Span<const uint8_t> literalInsns, ObjLiteralFlags flags) {
  bool isCow = flags.contains(ObjLiteralFlag::ArrayCOW);
  ObjLiteralReader reader(literalInsns);
  ObjLiteralInsn insn;

  Rooted<ValueVector> elements(cx, ValueVector(cx));

  while (reader.readInsn(&insn)) {
    MOZ_ASSERT(insn.isValid());

    JS::Value propVal = InterpretObjLiteralValue(atoms, insn);
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

JSObject* InterpretObjLiteral(JSContext* cx, const ObjLiteralAtomVector& atoms,
                              const mozilla::Span<const uint8_t> literalInsns,
                              ObjLiteralFlags flags) {
  return flags.contains(ObjLiteralFlag::Array)
             ? InterpretObjLiteralArray(cx, atoms, literalInsns, flags)
             : InterpretObjLiteralObj(cx, atoms, literalInsns, flags);
}

}  // namespace js
