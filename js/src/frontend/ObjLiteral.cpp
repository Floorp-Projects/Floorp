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
#include "vm/JSONPrinter.h"  // js::JSONPrinter
#include "vm/ObjectGroup.h"
#include "vm/Printer.h"  // js::Fprinter

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

#if defined(DEBUG) || defined(JS_JITSPEW)

static void DumpObjLiteralFlagsItems(js::JSONPrinter& json,
                                     ObjLiteralFlags flags) {
  if (flags.contains(ObjLiteralFlag::Array)) {
    json.value("Array");
    flags -= ObjLiteralFlag::Array;
  }
  if (flags.contains(ObjLiteralFlag::SpecificGroup)) {
    json.value("SpecificGroup");
    flags -= ObjLiteralFlag::SpecificGroup;
  }
  if (flags.contains(ObjLiteralFlag::Singleton)) {
    json.value("Singleton");
    flags -= ObjLiteralFlag::Singleton;
  }
  if (flags.contains(ObjLiteralFlag::ArrayCOW)) {
    json.value("ArrayCOW");
    flags -= ObjLiteralFlag::ArrayCOW;
  }
  if (flags.contains(ObjLiteralFlag::NoValues)) {
    json.value("NoValues");
    flags -= ObjLiteralFlag::NoValues;
  }
  if (flags.contains(ObjLiteralFlag::IsInnerSingleton)) {
    json.value("IsInnerSingleton");
    flags -= ObjLiteralFlag::IsInnerSingleton;
  }

  if (!flags.isEmpty()) {
    json.value("Unknown(%x)", flags.serialize());
  }
}

void ObjLiteralWriter::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void ObjLiteralWriter::dump(js::JSONPrinter& json) {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void ObjLiteralWriter::dumpFields(js::JSONPrinter& json) {
  json.beginListProperty("flags");
  DumpObjLiteralFlagsItems(json, flags_);
  json.endList();

  json.beginListProperty("code");
  ObjLiteralReader reader(getCode());
  ObjLiteralInsn insn;
  while (reader.readInsn(&insn)) {
    json.beginObject();

    if (insn.getKey().isNone()) {
      json.nullProperty("key");
    } else if (insn.getKey().isAtomIndex()) {
      uint32_t index = insn.getKey().getAtomIndex();
      json.formatProperty("key", "ConstAtom(%u)", index);
    } else if (insn.getKey().isArrayIndex()) {
      uint32_t index = insn.getKey().getArrayIndex();
      json.formatProperty("key", "ArrayIndex(%u)", index);
    }

    switch (insn.getOp()) {
      case ObjLiteralOpcode::ConstValue: {
        const Value& v = insn.getConstValue();
        json.formatProperty("op", "ConstValue(%f)", v.toNumber());
        break;
      }
      case ObjLiteralOpcode::ConstAtom: {
        uint32_t index = insn.getAtomIndex();
        json.formatProperty("op", "ConstAtom(%u)", index);
        break;
      }
      case ObjLiteralOpcode::Null:
        json.property("op", "Null");
        break;
      case ObjLiteralOpcode::Undefined:
        json.property("op", "Undefined");
        break;
      case ObjLiteralOpcode::True:
        json.property("op", "True");
        break;
      case ObjLiteralOpcode::False:
        json.property("op", "False");
        break;
      default:
        json.formatProperty("op", "Invalid(%x)", uint8_t(insn.getOp()));
        break;
    }

    json.endObject();
  }
  json.endList();
}

void ObjLiteralStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void ObjLiteralStencil::dump(js::JSONPrinter& json) {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void ObjLiteralStencil::dumpFields(js::JSONPrinter& json) {
  writer_.dumpFields(json);

  json.beginListProperty("atoms");
  for (auto& atom : atoms_) {
    if (atom) {
      GenericPrinter& out = json.beginString();
      atom->dumpCharsNoQuote(out);
      json.endString();
    } else {
      json.nullValue();
    }
  }
  json.endList();
}

#endif  // defined(DEBUG) || defined(JS_JITSPEW)

}  // namespace js
