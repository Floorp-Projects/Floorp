/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/BuiltinObjectKind.h"

#include "jspubtd.h"

#include "vm/GlobalObject.h"
#include "vm/JSContext.h"

using namespace js;

static JSProtoKey ToProtoKey(BuiltinObjectKind kind) {
  switch (kind) {
    case BuiltinObjectKind::Array:
      return JSProto_Array;
    case BuiltinObjectKind::ArrayBuffer:
      return JSProto_ArrayBuffer;
    case BuiltinObjectKind::Iterator:
      return JSProto_Iterator;
    case BuiltinObjectKind::Promise:
      return JSProto_Promise;
    case BuiltinObjectKind::RegExp:
      return JSProto_RegExp;
    case BuiltinObjectKind::SharedArrayBuffer:
      return JSProto_SharedArrayBuffer;

    case BuiltinObjectKind::FunctionPrototype:
      return JSProto_Function;
    case BuiltinObjectKind::ObjectPrototype:
      return JSProto_Object;
    case BuiltinObjectKind::RegExpPrototype:
      return JSProto_RegExp;
    case BuiltinObjectKind::StringPrototype:
      return JSProto_String;

    case BuiltinObjectKind::DateTimeFormat:
      return JSProto_DateTimeFormat;
    case BuiltinObjectKind::NumberFormat:
      return JSProto_NumberFormat;

    case BuiltinObjectKind::None:
      break;
  }
  MOZ_CRASH("Unexpected builtin object kind");
}

static bool IsPrototype(BuiltinObjectKind kind) {
  switch (kind) {
    case BuiltinObjectKind::Array:
    case BuiltinObjectKind::ArrayBuffer:
    case BuiltinObjectKind::Iterator:
    case BuiltinObjectKind::Promise:
    case BuiltinObjectKind::RegExp:
    case BuiltinObjectKind::SharedArrayBuffer:
      return false;

    case BuiltinObjectKind::FunctionPrototype:
    case BuiltinObjectKind::ObjectPrototype:
    case BuiltinObjectKind::RegExpPrototype:
    case BuiltinObjectKind::StringPrototype:
      return true;

    case BuiltinObjectKind::DateTimeFormat:
    case BuiltinObjectKind::NumberFormat:
      return false;

    case BuiltinObjectKind::None:
      break;
  }
  MOZ_CRASH("Unexpected builtin object kind");
}

using BuiltinName =
    const js::frontend::ParserName* js::frontend::WellKnownParserAtoms::*;

struct BuiltinObjectMap {
  BuiltinName name;
  BuiltinObjectKind kind;
};

BuiltinObjectKind js::BuiltinConstructorForName(
    JSContext* cx, const js::frontend::ParserAtom* name) {
  using WellKnownName = js::frontend::WellKnownParserAtoms;
  static constexpr BuiltinObjectMap constructors[] = {
      {&WellKnownName::Array, BuiltinObjectKind::Array},
      {&WellKnownName::ArrayBuffer, BuiltinObjectKind::ArrayBuffer},
      {&WellKnownName::Iterator, BuiltinObjectKind::Iterator},
      {&WellKnownName::Promise, BuiltinObjectKind::Promise},
      {&WellKnownName::RegExp, BuiltinObjectKind::RegExp},
      {&WellKnownName::SharedArrayBuffer, BuiltinObjectKind::SharedArrayBuffer},
      {&WellKnownName::DateTimeFormat, BuiltinObjectKind::DateTimeFormat},
      {&WellKnownName::NumberFormat, BuiltinObjectKind::NumberFormat},
  };

  for (auto& builtin : constructors) {
    if (name == cx->parserNames().*(builtin.name)) {
      return builtin.kind;
    }
  }
  return BuiltinObjectKind::None;
}

BuiltinObjectKind js::BuiltinPrototypeForName(
    JSContext* cx, const js::frontend::ParserAtom* name) {
  using WellKnownName = js::frontend::WellKnownParserAtoms;
  static constexpr BuiltinObjectMap prototypes[] = {
      {&WellKnownName::Function, BuiltinObjectKind::FunctionPrototype},
      {&WellKnownName::Object, BuiltinObjectKind::ObjectPrototype},
      {&WellKnownName::RegExp, BuiltinObjectKind::RegExpPrototype},
      {&WellKnownName::String, BuiltinObjectKind::StringPrototype},
  };

  for (auto& builtin : prototypes) {
    if (name == cx->parserNames().*(builtin.name)) {
      return builtin.kind;
    }
  }
  return BuiltinObjectKind::None;
}

JSObject* js::MaybeGetBuiltinObject(GlobalObject* global,
                                    BuiltinObjectKind kind) {
  JSProtoKey key = ToProtoKey(kind);
  if (IsPrototype(kind)) {
    return global->maybeGetPrototype(key);
  }
  return global->maybeGetConstructor(key);
}

JSObject* js::GetOrCreateBuiltinObject(JSContext* cx, BuiltinObjectKind kind) {
  JSProtoKey key = ToProtoKey(kind);
  if (IsPrototype(kind)) {
    return GlobalObject::getOrCreatePrototype(cx, key);
  }
  return GlobalObject::getOrCreateConstructor(cx, key);
}

const char* js::BuiltinObjectName(BuiltinObjectKind kind) {
  switch (kind) {
    case BuiltinObjectKind::Array:
      return "Array";
    case BuiltinObjectKind::ArrayBuffer:
      return "ArrayBuffer";
    case BuiltinObjectKind::Iterator:
      return "Iterator";
    case BuiltinObjectKind::Promise:
      return "Promise";
    case BuiltinObjectKind::RegExp:
      return "RegExp";
    case BuiltinObjectKind::SharedArrayBuffer:
      return "SharedArrayBuffer";

    case BuiltinObjectKind::FunctionPrototype:
      return "Function.prototype";
    case BuiltinObjectKind::ObjectPrototype:
      return "Object.prototype";
    case BuiltinObjectKind::RegExpPrototype:
      return "RegExp.prototype";
    case BuiltinObjectKind::StringPrototype:
      return "String.prototype";

    case BuiltinObjectKind::DateTimeFormat:
      return "DateTimeFormat";
    case BuiltinObjectKind::NumberFormat:
      return "NumberFormat";

    case BuiltinObjectKind::None:
      break;
  }
  MOZ_CRASH("Unexpected builtin object kind");
}
