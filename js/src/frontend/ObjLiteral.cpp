/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=0 ft=c:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ObjLiteral.h"

#include "mozilla/DebugOnly.h"

#include "frontend/CompilationInfo.h"  // frontend::CompilationAtomCache
#include "frontend/ParserAtom.h"  // frontend::ParserAtom, frontend::ParserAtomTable
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

static void InterpretObjLiteralValue(JSContext* cx,
                                     frontend::CompilationAtomCache& atomCache,
                                     const ObjLiteralInsn& insn,
                                     JS::Value* valOut) {
  switch (insn.getOp()) {
    case ObjLiteralOpcode::ConstValue:
      *valOut = insn.getConstValue();
      return;
    case ObjLiteralOpcode::ConstAtom: {
      frontend::TaggedParserAtomIndex index = insn.getAtomIndex();
      JSAtom* jsatom = atomCache.getExistingAtomAt(cx, index);
      MOZ_ASSERT(jsatom);
      *valOut = StringValue(jsatom);
      return;
    }
    case ObjLiteralOpcode::Null:
      *valOut = NullValue();
      return;
    case ObjLiteralOpcode::Undefined:
      *valOut = UndefinedValue();
      return;
    case ObjLiteralOpcode::True:
      *valOut = BooleanValue(true);
      return;
    case ObjLiteralOpcode::False:
      *valOut = BooleanValue(false);
      return;
    default:
      MOZ_CRASH("Unexpected object-literal instruction opcode");
  }
}

static JSObject* InterpretObjLiteralObj(
    JSContext* cx, frontend::CompilationAtomCache& atomCache,
    const mozilla::Span<const uint8_t> literalInsns, ObjLiteralFlags flags) {
  bool singleton = flags.contains(ObjLiteralFlag::Singleton);

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
      JSAtom* jsatom =
          atomCache.getExistingAtomAt(cx, insn.getKey().getAtomIndex());
      MOZ_ASSERT(jsatom);
      propId = AtomToId(jsatom);
    }

    JS::Value propVal;
    if (singleton) {
      InterpretObjLiteralValue(cx, atomCache, insn, &propVal);
    }

    if (!properties.emplaceBack(propId, propVal)) {
      return nullptr;
    }
  }

  return NewPlainObjectWithProperties(cx, properties.begin(),
                                      properties.length(), TenuredObject);
}

static JSObject* InterpretObjLiteralArray(
    JSContext* cx, frontend::CompilationAtomCache& atomCache,
    const mozilla::Span<const uint8_t> literalInsns, ObjLiteralFlags flags) {
  ObjLiteralReader reader(literalInsns);
  ObjLiteralInsn insn;

  Rooted<ValueVector> elements(cx, ValueVector(cx));

  while (reader.readInsn(&insn)) {
    MOZ_ASSERT(insn.isValid());

    JS::Value propVal;
    InterpretObjLiteralValue(cx, atomCache, insn, &propVal);
    if (!elements.append(propVal)) {
      return nullptr;
    }
  }

  return NewDenseCopiedArray(cx, elements.length(), elements.begin(),
                             /* proto = */ nullptr,
                             NewObjectKind::TenuredObject);
}

JSObject* InterpretObjLiteral(JSContext* cx,
                              frontend::CompilationAtomCache& atomCache,
                              const mozilla::Span<const uint8_t> literalInsns,
                              ObjLiteralFlags flags) {
  return flags.contains(ObjLiteralFlag::Array)
             ? InterpretObjLiteralArray(cx, atomCache, literalInsns, flags)
             : InterpretObjLiteralObj(cx, atomCache, literalInsns, flags);
}

#if defined(DEBUG) || defined(JS_JITSPEW)

static void DumpObjLiteralFlagsItems(js::JSONPrinter& json,
                                     ObjLiteralFlags flags) {
  if (flags.contains(ObjLiteralFlag::Array)) {
    json.value("Array");
    flags -= ObjLiteralFlag::Array;
  }
  if (flags.contains(ObjLiteralFlag::Singleton)) {
    json.value("Singleton");
    flags -= ObjLiteralFlag::Singleton;
  }

  if (!flags.isEmpty()) {
    json.value("Unknown(%x)", flags.serialize());
  }
}

static void DumpObjLiteral(js::JSONPrinter& json,
                           frontend::BaseCompilationStencil* stencil,
                           mozilla::Span<const uint8_t> code,
                           const ObjLiteralFlags& flags) {
  json.beginListProperty("flags");
  DumpObjLiteralFlagsItems(json, flags);
  json.endList();

  json.beginListProperty("code");
  ObjLiteralReader reader(code);
  ObjLiteralInsn insn;
  while (reader.readInsn(&insn)) {
    json.beginObject();

    if (insn.getKey().isNone()) {
      json.nullProperty("key");
    } else if (insn.getKey().isAtomIndex()) {
      frontend::TaggedParserAtomIndex index = insn.getKey().getAtomIndex();
      json.beginObjectProperty("key");
      DumpTaggedParserAtomIndex(json, index, stencil);
      json.endObject();
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
        frontend::TaggedParserAtomIndex index = insn.getAtomIndex();
        json.beginObjectProperty("op");
        DumpTaggedParserAtomIndex(json, index, stencil);
        json.endObject();
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

void ObjLiteralWriter::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void ObjLiteralWriter::dump(js::JSONPrinter& json,
                            frontend::BaseCompilationStencil* stencil) {
  json.beginObject();
  dumpFields(json, stencil);
  json.endObject();
}

void ObjLiteralWriter::dumpFields(js::JSONPrinter& json,
                                  frontend::BaseCompilationStencil* stencil) {
  DumpObjLiteral(json, stencil, getCode(), flags_);
}

void ObjLiteralStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void ObjLiteralStencil::dump(js::JSONPrinter& json,
                             frontend::BaseCompilationStencil* stencil) {
  json.beginObject();
  dumpFields(json, stencil);
  json.endObject();
}

void ObjLiteralStencil::dumpFields(js::JSONPrinter& json,
                                   frontend::BaseCompilationStencil* stencil) {
  DumpObjLiteral(json, stencil, code_, flags_);
}

#endif  // defined(DEBUG) || defined(JS_JITSPEW)

}  // namespace js
