/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ToSource.h"

#include "mozilla/ArrayUtils.h"     // mozilla::ArrayLength
#include "mozilla/Assertions.h"     // MOZ_ASSERT
#include "mozilla/FloatingPoint.h"  // mozilla::IsNegativeZero

#include <stdint.h>  // uint32_t

#include "jsfriendapi.h"  // CheckRecursionLimit

#include "builtin/Array.h"    // ArrayToSource
#include "builtin/Boolean.h"  // BooleanToString
#include "builtin/Object.h"   // ObjectToSource
#include "gc/Allocator.h"     // CanGC
#include "js/Symbol.h"        // SymbolCode, JS::WellKnownSymbolLimit
#include "js/TypeDecls.h"  // Rooted{Function, Object, String, Value}, HandleValue, Latin1Char
#include "js/Utility.h"         // UniqueChars
#include "js/Value.h"           // JS::Value
#include "util/StringBuffer.h"  // JSStringBuilder
#include "vm/ArrayObject.h"     // ArrayObject
#include "vm/ErrorObject.h"     // ErrorObject, ErrorToSource
#include "vm/Interpreter.h"     // Call
#include "vm/JSContext.h"       // JSContext
#include "vm/JSFunction.h"      // JSFunction, FunctionToString
#include "vm/Printer.h"         // QuoteString
#include "vm/RegExpObject.h"    // RegExpObject
#include "vm/SelfHosting.h"     // CallSelfHostedFunction
#include "vm/Stack.h"           // FixedInvokeArgs
#include "vm/StringType.h"      // NewStringCopy{N,Z}, ToString
#include "vm/SymbolType.h"      // Symbol

#include "vm/JSContext-inl.h"         // JSContext::check
#include "vm/JSObject-inl.h"          // IsCallable
#include "vm/ObjectOperations-inl.h"  // GetProperty

using namespace js;

using mozilla::IsNegativeZero;

/*
 * Convert a JSString to its source expression; returns null after reporting an
 * error, otherwise returns a new string reference. No Handle needed since the
 * input is dead after the GC.
 */
static JSString* StringToSource(JSContext* cx, JSString* str) {
  UniqueChars chars = QuoteString(cx, str, '"');
  if (!chars) {
    return nullptr;
  }
  return NewStringCopyZ<CanGC>(cx, chars.get());
}

static JSString* SymbolToSource(JSContext* cx, Symbol* symbol) {
  RootedString desc(cx, symbol->description());
  SymbolCode code = symbol->code();
  if (code != SymbolCode::InSymbolRegistry &&
      code != SymbolCode::UniqueSymbol) {
    // Well-known symbol.
    MOZ_ASSERT(uint32_t(code) < JS::WellKnownSymbolLimit);
    return desc;
  }

  JSStringBuilder buf(cx);
  if (code == SymbolCode::InSymbolRegistry ? !buf.append("Symbol.for(")
                                           : !buf.append("Symbol(")) {
    return nullptr;
  }
  if (desc) {
    UniqueChars quoted = QuoteString(cx, desc, '"');
    if (!quoted || !buf.append(quoted.get(), strlen(quoted.get()))) {
      return nullptr;
    }
  }
  if (!buf.append(')')) {
    return nullptr;
  }
  return buf.finishString();
}

JSString* js::ValueToSource(JSContext* cx, HandleValue v) {
  if (!CheckRecursionLimit(cx)) {
    return nullptr;
  }
  cx->check(v);

  switch (v.type()) {
    case JS::ValueType::Undefined:
      return cx->names().void0;

    case JS::ValueType::String:
      return StringToSource(cx, v.toString());

    case JS::ValueType::Symbol:
      return SymbolToSource(cx, v.toSymbol());

    case JS::ValueType::Null:
      return cx->names().null;

    case JS::ValueType::Boolean:
      return BooleanToString(cx, v.toBoolean());

    case JS::ValueType::Double:
      /* Special case to preserve negative zero, _contra_ toString. */
      if (IsNegativeZero(v.toDouble())) {
        static const Latin1Char negativeZero[] = {'-', '0'};

        return NewStringCopyN<CanGC>(cx, negativeZero,
                                     mozilla::ArrayLength(negativeZero));
      }
      [[fallthrough]];
    case JS::ValueType::Int32:
      return ToString<CanGC>(cx, v);

    case JS::ValueType::BigInt: {
      RootedString str(cx, ToString<CanGC>(cx, v));
      if (!str) {
        return nullptr;
      }

      RootedString n(cx, cx->staticStrings().getUnit('n'));

      return ConcatStrings<CanGC>(cx, str, n);
    }

    case JS::ValueType::Object: {
      RootedValue fval(cx);
      RootedObject obj(cx, &v.toObject());
      if (!GetProperty(cx, obj, obj, cx->names().toSource, &fval)) {
        return nullptr;
      }
      if (IsCallable(fval)) {
        RootedValue v(cx);
        if (!js::Call(cx, fval, obj, &v)) {
          return nullptr;
        }

        return ToString<CanGC>(cx, v);
      }

      if (obj->is<JSFunction>()) {
        RootedFunction fun(cx, &obj->as<JSFunction>());
        return FunctionToString(cx, fun, true);
      }

      if (obj->is<ArrayObject>()) {
        return ArrayToSource(cx, obj);
      }

      if (obj->is<ErrorObject>()) {
        return ErrorToSource(cx, obj);
      }

      if (obj->is<RegExpObject>()) {
        FixedInvokeArgs<0> args(cx);
        RootedValue rval(cx);
        if (!CallSelfHostedFunction(cx, cx->names().RegExpToString, v, args,
                                    &rval)) {
          return nullptr;
        }
        return ToString<CanGC>(cx, rval);
      }

      return ObjectToSource(cx, obj);
    }

    case JS::ValueType::PrivateGCThing:
    case JS::ValueType::Magic:
      MOZ_ASSERT_UNREACHABLE(
          "internal value types shouldn't leak into places "
          "wanting source representations");
      return nullptr;
  }

  MOZ_ASSERT_UNREACHABLE("shouldn't see an unrecognized value type");
  return nullptr;
}
