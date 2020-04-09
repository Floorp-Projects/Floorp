/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmJS.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/RangedPtr.h"

#include <algorithm>

#include "builtin/TypedObject.h"
#include "gc/FreeOp.h"
#include "jit/AtomicOperations.h"
#include "jit/JitOptions.h"
#include "jit/Simulator.h"
#include "js/Printf.h"
#include "js/PropertySpec.h"  // JS_{PS,FN}{,_END}
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/ErrorObject.h"
#include "vm/Interpreter.h"
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/StringType.h"
#include "vm/Warnings.h"  // js::WarnNumberASCII
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmCraneliftCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmProcess.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmValidate.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::MakeSpan;
using mozilla::Nothing;
using mozilla::RangedPtr;

extern mozilla::Atomic<bool> fuzzingSafe;

static inline bool WasmMultiValueFlag(JSContext* cx) {
#ifdef ENABLE_WASM_MULTI_VALUE
  return cx->options().wasmMultiValue();
#else
  return false;
#endif
}

// Compiler availability predicates.  These must be kept in sync with the
// feature predicates in the next section below.
//
// These can't call the feature predicates since the feature predicates call
// back to these predicates.  So there will be a small amount of duplicated
// logic here, but as compilers reach feature parity that duplication will go
// away.
//
// There's a static precedence order between the optimizing compilers.  This
// order currently ranks Cranelift over Ion on all platforms because Cranelift
// is disabled by default on all platforms: anyone who has enabled Cranelift
// will wish to use it instead of Ion.
//
// The precedence order is implemented by guards in IonAvailable() and
// CraneliftAvailable().  We expect that it will become more complex as the
// default settings change.  But it should remain static.

bool wasm::BaselineAvailable(JSContext* cx) {
  // Baseline supports every feature supported by any compiler.
  return cx->options().wasmBaseline() && BaselinePlatformSupport();
}

bool wasm::IonAvailable(JSContext* cx) {
  if (!cx->options().wasmIon() || !IonPlatformSupport()) {
    return false;
  }
  bool isDisabled = false;
  MOZ_ALWAYS_TRUE(IonDisabledByFeatures(cx, &isDisabled));
  return !isDisabled && !CraneliftAvailable(cx);
}

template <size_t ArrayLength>
static inline bool Append(JSStringBuilder* reason, const char (&s)[ArrayLength],
                          char* sep) {
  if ((*sep && !reason->append(*sep)) || !reason->append(s)) {
    return false;
  }
  *sep = ',';
  return true;
}

bool wasm::IonDisabledByFeatures(JSContext* cx, bool* isDisabled,
                                 JSStringBuilder* reason) {
  // Ion has no debugging support, no gc support.
  bool debug = cx->realm() && cx->realm()->debuggerObservesAsmJS();
  bool gc = cx->options().wasmGc();
  if (reason) {
    char sep = 0;
    if (debug && !Append(reason, "debug", &sep)) {
      return false;
    }
    if (gc && !Append(reason, "gc", &sep)) {
      return false;
    }
  }
  *isDisabled = debug || gc;
  return true;
}

bool wasm::CraneliftAvailable(JSContext* cx) {
  if (!cx->options().wasmCranelift() || !CraneliftPlatformSupport()) {
    return false;
  }
  bool isDisabled = false;
  MOZ_ALWAYS_TRUE(CraneliftDisabledByFeatures(cx, &isDisabled));
  return !isDisabled;
}

bool wasm::CraneliftDisabledByFeatures(JSContext* cx, bool* isDisabled,
                                       JSStringBuilder* reason) {
  // Cranelift has no debugging support, no gc support, no multi-value support,
  // no threads.
  bool debug = cx->realm() && cx->realm()->debuggerObservesAsmJS();
  bool gc = cx->options().wasmGc();
  bool multiValue = WasmMultiValueFlag(cx);
  bool threads =
      cx->realm() &&
      cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled();
  if (reason) {
    char sep = 0;
    if (debug && !Append(reason, "debug", &sep)) {
      return false;
    }
    if (gc && !Append(reason, "gc", &sep)) {
      return false;
    }
    if (multiValue && !Append(reason, "multi-value", &sep)) {
      return false;
    }
    if (threads && !Append(reason, "threads", &sep)) {
      return false;
    }
  }
  *isDisabled = debug || gc || multiValue || threads;
  return true;
}

// Feature predicates.  These must be kept in sync with the predicates in the
// section above.
//
// The meaning of these predicates is tricky: A predicate is true for a feature
// if the feature is enabled and/or compiled-in *and* we have *at least one*
// compiler that can support the feature.  Subsequent compiler selection must
// ensure that only compilers that actually support the feature are used.

bool wasm::ReftypesAvailable(JSContext* cx) {
  // All compilers support reference types.
#ifdef ENABLE_WASM_REFTYPES
  return true;
#else
  return false;
#endif
}

bool wasm::GcTypesAvailable(JSContext* cx) {
  // Cranelift and Ion do not support GC.
  return cx->options().wasmGc() && BaselineAvailable(cx);
}

bool wasm::MultiValuesAvailable(JSContext* cx) {
  // Cranelift does not support multi-value.
  return WasmMultiValueFlag(cx) && (BaselineAvailable(cx) || IonAvailable(cx));
}

bool wasm::I64BigIntConversionAvailable(JSContext* cx) {
  // All compilers support int64<->bigint conversion.
#ifdef ENABLE_WASM_BIGINT
  return cx->options().isWasmBigIntEnabled();
#else
  return false;
#endif
}

bool wasm::ThreadsAvailable(JSContext* cx) {
  // Cranelift does not support atomics.
  return cx->realm() &&
         cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled() &&
         (BaselineAvailable(cx) || IonAvailable(cx));
}

bool wasm::HasPlatformSupport(JSContext* cx) {
#if !MOZ_LITTLE_ENDIAN() || defined(JS_CODEGEN_NONE)
  return false;
#endif

  if (gc::SystemPageSize() > wasm::PageSize) {
    return false;
  }

  if (!JitOptions.supportsFloatingPoint) {
    return false;
  }

  if (!JitOptions.supportsUnalignedAccesses) {
    return false;
  }

  if (!wasm::EnsureFullSignalHandlers(cx)) {
    return false;
  }

  // Wasm threads require 8-byte lock-free atomics.
  if (!jit::AtomicOperations::isLockfree8()) {
    return false;
  }

#ifdef JS_SIMULATOR
  if (!Simulator::supportsAtomics()) {
    return false;
  }
#endif

  // Test only whether the compilers are supported on the hardware, not whether
  // they are enabled.
  return BaselinePlatformSupport() || IonPlatformSupport() ||
         CraneliftPlatformSupport();
}

bool wasm::HasSupport(JSContext* cx) {
  // If the general wasm pref is on, it's on for everything.
  bool prefEnabled = cx->options().wasm();
  // If the general pref is off, check trusted principals.
  if (MOZ_UNLIKELY(!prefEnabled)) {
    prefEnabled = cx->options().wasmForTrustedPrinciples() && cx->realm() &&
                  cx->realm()->principals() &&
                  cx->realm()->principals()->isSystemOrAddonPrincipal();
  }
  return prefEnabled && HasPlatformSupport(cx) &&
         (BaselineAvailable(cx) || IonAvailable(cx) || CraneliftAvailable(cx));
}

bool wasm::StreamingCompilationAvailable(JSContext* cx) {
  // This should match EnsureStreamSupport().
  return HasSupport(cx) &&
         cx->runtime()->offThreadPromiseState.ref().initialized() &&
         CanUseExtraThreads() && cx->runtime()->consumeStreamCallback &&
         cx->runtime()->reportStreamErrorCallback;
}

bool wasm::CodeCachingAvailable(JSContext* cx) {
  // At the moment, we require Ion support for code caching.  The main reason
  // for this is that wasm::CompileAndSerialize() does not have access to
  // information about which optimizing compiler it should use.  See comments in
  // CompileAndSerialize(), below.
  return StreamingCompilationAvailable(cx) && IonAvailable(cx);
}

bool wasm::CheckRefType(JSContext* cx, RefType::Kind targetTypeKind,
                        HandleValue v, MutableHandleFunction fnval,
                        MutableHandleAnyRef refval) {
  switch (targetTypeKind) {
    case RefType::Func:
      if (!CheckFuncRefValue(cx, v, fnval)) {
        return false;
      }
      break;
    case RefType::Null:
      if (!v.isNull()) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_WASM_NULL_REQUIRED);
        return false;
      }
      refval.set(AnyRef::null());
      break;
    case RefType::Any:
      if (!BoxAnyRef(cx, v, refval)) {
        return false;
      }
      break;
    case RefType::TypeIndex:
      MOZ_CRASH("temporarily unsupported Ref type");
  }
  return true;
}

static bool ToWebAssemblyValue(JSContext* cx, ValType targetType, HandleValue v,
                               MutableHandleVal val) {
  switch (targetType.kind()) {
    case ValType::I32: {
      int32_t i32;
      if (!ToInt32(cx, v, &i32)) {
        return false;
      }
      val.set(Val(uint32_t(i32)));
      return true;
    }
    case ValType::F32: {
      double d;
      if (!ToNumber(cx, v, &d)) {
        return false;
      }
      val.set(Val(float(d)));
      return true;
    }
    case ValType::F64: {
      double d;
      if (!ToNumber(cx, v, &d)) {
        return false;
      }
      val.set(Val(d));
      return true;
    }
    case ValType::I64: {
#ifdef ENABLE_WASM_BIGINT
      if (I64BigIntConversionAvailable(cx)) {
        BigInt* bigint = ToBigInt(cx, v);
        if (!bigint) {
          return false;
        }
        val.set(Val(BigInt::toUint64(bigint)));
        return true;
      }
#endif
      break;
    }
    case ValType::Ref: {
      RootedFunction fun(cx);
      RootedAnyRef any(cx, AnyRef::null());
      if (!CheckRefType(cx, targetType.refTypeKind(), v, &fun, &any)) {
        return false;
      }
      switch (targetType.refTypeKind()) {
        case RefType::Func:
          val.set(Val(RefType::func(), FuncRef::fromJSFunction(fun)));
          return true;
        case RefType::Any:
        case RefType::Null:
          val.set(Val(targetType.refType(), any));
          return true;
        case RefType::TypeIndex:
          break;
      }
      break;
    }
  }
  MOZ_CRASH("unexpected import value type, caller must guard");
}

static bool ToJSValue(JSContext* cx, const Val& val, MutableHandleValue out) {
  switch (val.type().kind()) {
    case ValType::I32:
      out.setInt32(val.i32());
      return true;
    case ValType::F32:
      out.setDouble(JS::CanonicalizeNaN(double(val.f32())));
      return true;
    case ValType::F64:
      out.setDouble(JS::CanonicalizeNaN(val.f64()));
      return true;
    case ValType::I64: {
#ifdef ENABLE_WASM_BIGINT
      if (I64BigIntConversionAvailable(cx)) {
        BigInt* bi = BigInt::createFromInt64(cx, val.i64());
        if (!bi) {
          return false;
        }
        out.setBigInt(bi);
        return true;
      }
#endif
      break;
    }
    case ValType::Ref:
      switch (val.type().refTypeKind()) {
        case RefType::Func:
          out.set(UnboxFuncRef(FuncRef::fromAnyRefUnchecked(val.ref())));
          return true;
        case RefType::Any:
        case RefType::Null:
          out.set(UnboxAnyRef(val.ref()));
          return true;
        case RefType::TypeIndex:
          break;
      }
      break;
  }
  MOZ_CRASH("unexpected type when translating to a JS value");
}

// ============================================================================
// Imports

static bool ThrowBadImportArg(JSContext* cx) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_IMPORT_ARG);
  return false;
}

static bool ThrowBadImportType(JSContext* cx, const char* field,
                               const char* str) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_IMPORT_TYPE, field, str);
  return false;
}

static bool GetProperty(JSContext* cx, HandleObject obj, const char* chars,
                        MutableHandleValue v) {
  JSAtom* atom = AtomizeUTF8Chars(cx, chars, strlen(chars));
  if (!atom) {
    return false;
  }

  RootedId id(cx, AtomToId(atom));
  return GetProperty(cx, obj, obj, id, v);
}

bool js::wasm::GetImports(JSContext* cx, const Module& module,
                          HandleObject importObj, ImportValues* imports) {
  if (!module.imports().empty() && !importObj) {
    return ThrowBadImportArg(cx);
  }

  const Metadata& metadata = module.metadata();

  uint32_t globalIndex = 0;
  const GlobalDescVector& globals = metadata.globals;
  uint32_t tableIndex = 0;
  const TableDescVector& tables = metadata.tables;
  for (const Import& import : module.imports()) {
    RootedValue v(cx);
    if (!GetProperty(cx, importObj, import.module.get(), &v)) {
      return false;
    }

    if (!v.isObject()) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_IMPORT_FIELD,
                               import.module.get());
      return false;
    }

    RootedObject obj(cx, &v.toObject());
    if (!GetProperty(cx, obj, import.field.get(), &v)) {
      return false;
    }

    switch (import.kind) {
      case DefinitionKind::Function: {
        if (!IsFunctionObject(v)) {
          return ThrowBadImportType(cx, import.field.get(), "Function");
        }

        if (!imports->funcs.append(&v.toObject().as<JSFunction>())) {
          return false;
        }

        break;
      }
      case DefinitionKind::Table: {
        const uint32_t index = tableIndex++;
        if (!v.isObject() || !v.toObject().is<WasmTableObject>()) {
          return ThrowBadImportType(cx, import.field.get(), "Table");
        }

        RootedWasmTableObject obj(cx, &v.toObject().as<WasmTableObject>());
        if (obj->table().kind() != tables[index].kind) {
          JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                   JSMSG_WASM_BAD_TBL_TYPE_LINK);
          return false;
        }

        if (!imports->tables.append(obj)) {
          return false;
        }
        break;
      }
      case DefinitionKind::Memory: {
        if (!v.isObject() || !v.toObject().is<WasmMemoryObject>()) {
          return ThrowBadImportType(cx, import.field.get(), "Memory");
        }

        MOZ_ASSERT(!imports->memory);
        imports->memory = &v.toObject().as<WasmMemoryObject>();
        break;
      }
      case DefinitionKind::Global: {
        const uint32_t index = globalIndex++;
        const GlobalDesc& global = globals[index];
        MOZ_ASSERT(global.importIndex() == index);

        RootedVal val(cx);
        if (v.isObject() && v.toObject().is<WasmGlobalObject>()) {
          RootedWasmGlobalObject obj(cx, &v.toObject().as<WasmGlobalObject>());

          if (obj->isMutable() != global.isMutable()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_MUT_LINK);
            return false;
          }
          if (obj->type() != global.type()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_TYPE_LINK);
            return false;
          }

          if (imports->globalObjs.length() <= index &&
              !imports->globalObjs.resize(index + 1)) {
            ReportOutOfMemory(cx);
            return false;
          }
          imports->globalObjs[index] = obj;
          obj->val(&val);
        } else {
          if (IsNumberType(global.type())) {
            if (I64BigIntConversionAvailable(cx)) {
              if (global.type() == ValType::I64 && v.isNumber()) {
                return ThrowBadImportType(cx, import.field.get(), "BigInt");
              }
              if (global.type() != ValType::I64 && !v.isNumber()) {
                return ThrowBadImportType(cx, import.field.get(), "Number");
              }
            } else {
              if (!v.isNumber()) {
                return ThrowBadImportType(cx, import.field.get(), "Number");
              }
              if (global.type() == ValType::I64) {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                         JSMSG_WASM_BAD_I64_LINK);
                return false;
              }
            }
          } else {
            MOZ_ASSERT(global.type().isReference());
            if (!global.type().isAnyRef() && !v.isObjectOrNull()) {
              return ThrowBadImportType(cx, import.field.get(),
                                        "Object-or-null value required for "
                                        "non-anyref reference type");
            }
          }

          if (global.isMutable()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_MUT_LINK);
            return false;
          }

          if (!ToWebAssemblyValue(cx, global.type(), v, &val)) {
            return false;
          }
        }

        if (!imports->globalValues.append(val)) {
          return false;
        }

        break;
      }
    }
  }

  MOZ_ASSERT(globalIndex == globals.length() ||
             !globals[globalIndex].isImport());

  return true;
}

static bool DescribeScriptedCaller(JSContext* cx, ScriptedCaller* caller,
                                   const char* introducer) {
  // Note: JS::DescribeScriptedCaller returns whether a scripted caller was
  // found, not whether an error was thrown. This wrapper function converts
  // back to the more ordinary false-if-error form.

  JS::AutoFilename af;
  if (JS::DescribeScriptedCaller(cx, &af, &caller->line)) {
    caller->filename =
        FormatIntroducedFilename(cx, af.get(), caller->line, introducer);
    if (!caller->filename) {
      return false;
    }
  }

  return true;
}

// ============================================================================
// Testing / Fuzzing support

bool wasm::Eval(JSContext* cx, Handle<TypedArrayObject*> code,
                HandleObject importObj,
                MutableHandleWasmInstanceObject instanceObj) {
  if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_WebAssembly)) {
    return false;
  }

  MutableBytes bytecode = cx->new_<ShareableBytes>();
  if (!bytecode) {
    return false;
  }

  if (!bytecode->append((uint8_t*)code->dataPointerEither().unwrap(),
                        code->byteLength())) {
    ReportOutOfMemory(cx);
    return false;
  }

  ScriptedCaller scriptedCaller;
  if (!DescribeScriptedCaller(cx, &scriptedCaller, "wasm_eval")) {
    return false;
  }

  SharedCompileArgs compileArgs =
      CompileArgs::build(cx, std::move(scriptedCaller));
  if (!compileArgs) {
    return false;
  }

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, *bytecode, &error, &warnings);
  if (!module) {
    if (error) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_COMPILE_ERROR, error.get());
      return false;
    }
    ReportOutOfMemory(cx);
    return false;
  }

  Rooted<ImportValues> imports(cx);
  if (!GetImports(cx, *module, importObj, imports.address())) {
    return false;
  }

  return module->instantiate(cx, imports.get(), nullptr, instanceObj);
}

struct MOZ_STACK_CLASS SerializeListener : JS::OptimizedEncodingListener {
  // MOZ_STACK_CLASS means these can be nops.
  MozExternalRefCountType MOZ_XPCOM_ABI AddRef() override { return 0; }
  MozExternalRefCountType MOZ_XPCOM_ABI Release() override { return 0; }

  DebugOnly<bool> called = false;
  Bytes* serialized;
  explicit SerializeListener(Bytes* serialized) : serialized(serialized) {}

  void storeOptimizedEncoding(JS::UniqueOptimizedEncodingBytes bytes) override {
    MOZ_ASSERT(!called);
    called = true;
    if (serialized->resize(bytes->length())) {
      memcpy(serialized->begin(), bytes->begin(), bytes->length());
    }
  }
};

bool wasm::CompileAndSerialize(const ShareableBytes& bytecode,
                               Bytes* serialized) {
  MutableCompileArgs compileArgs = js_new<CompileArgs>(ScriptedCaller());
  if (!compileArgs) {
    return false;
  }

  // The caller has ensured CodeCachingAvailable(). Moreover, we want to ensure
  // we go straight to tier-2 so that we synchronously call
  // JS::OptimizedEncodingListener::storeOptimizedEncoding().
  compileArgs->baselineEnabled = false;

  // We always pick Ion here, and we depend on CodeCachingAvailable() having
  // determined that Ion is available, see comments at CodeCachingAvailable().
  // To do better, we need to pass information about which compiler that should
  // be used into CompileAndSerialize().
  compileArgs->ionEnabled = true;

  // The caller must ensure that huge memory support is configured the same in
  // the receiving process of this serialized module.
  compileArgs->hugeMemory = wasm::IsHugeMemoryEnabled();

  SerializeListener listener(serialized);

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, bytecode, &error, &warnings, &listener);
  if (!module) {
    fprintf(stderr, "Compilation error: %s\n", error ? error.get() : "oom");
    return false;
  }

  MOZ_ASSERT(module->code().hasTier(Tier::Serialized));
  MOZ_ASSERT(listener.called);
  return !listener.serialized->empty();
}

bool wasm::DeserializeModule(JSContext* cx, const Bytes& serialized,
                             MutableHandleObject moduleObj) {
  MutableModule module =
      Module::deserialize(serialized.begin(), serialized.length());
  if (!module) {
    ReportOutOfMemory(cx);
    return false;
  }

  moduleObj.set(module->createObject(cx));
  return !!moduleObj;
}

// ============================================================================
// Common functions

// '[EnforceRange] unsigned long' types are coerced with
//    ConvertToInt(v, 32, 'unsigned')
// defined in Web IDL Section 3.2.4.9.
static bool EnforceRangeU32(JSContext* cx, HandleValue v, const char* kind,
                            const char* noun, uint32_t* u32) {
  // Step 4.
  double x;
  if (!ToNumber(cx, v, &x)) {
    return false;
  }

  // Step 5.
  if (mozilla::IsNegativeZero(x)) {
    x = 0.0;
  }

  // Step 6.1.
  if (!mozilla::IsFinite(x)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_UINT32, kind, noun);
    return false;
  }

  // Step 6.2.
  x = JS::ToInteger(x);

  // Step 6.3.
  if (x < 0 || x > double(UINT32_MAX)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_UINT32, kind, noun);
    return false;
  }

  *u32 = uint32_t(x);
  MOZ_ASSERT(double(*u32) == x);
  return true;
}

static bool GetLimits(JSContext* cx, HandleObject obj, uint32_t maxInitial,
                      uint32_t maxMaximum, const char* kind, Limits* limits,
                      Shareable allowShared) {
  JSAtom* initialAtom = Atomize(cx, "initial", strlen("initial"));
  if (!initialAtom) {
    return false;
  }
  RootedId initialId(cx, AtomToId(initialAtom));

  RootedValue initialVal(cx);
  if (!GetProperty(cx, obj, obj, initialId, &initialVal)) {
    return false;
  }

  if (!EnforceRangeU32(cx, initialVal, kind, "initial size",
                       &limits->initial)) {
    return false;
  }

  if (limits->initial > maxInitial) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_RANGE,
                             kind, "initial size");
    return false;
  }

  JSAtom* maximumAtom = Atomize(cx, "maximum", strlen("maximum"));
  if (!maximumAtom) {
    return false;
  }
  RootedId maximumId(cx, AtomToId(maximumAtom));

  RootedValue maxVal(cx);
  if (!GetProperty(cx, obj, obj, maximumId, &maxVal)) {
    return false;
  }

  // maxVal does not have a default value.
  if (!maxVal.isUndefined()) {
    limits->maximum.emplace();
    if (!EnforceRangeU32(cx, maxVal, kind, "maximum size",
                         limits->maximum.ptr())) {
      return false;
    }

    if (*limits->maximum > maxMaximum || limits->initial > *limits->maximum) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_RANGE, kind, "maximum size");
      return false;
    }
  }

  limits->shared = Shareable::False;

  if (allowShared == Shareable::True) {
    JSAtom* sharedAtom = Atomize(cx, "shared", strlen("shared"));
    if (!sharedAtom) {
      return false;
    }
    RootedId sharedId(cx, AtomToId(sharedAtom));

    RootedValue sharedVal(cx);
    if (!GetProperty(cx, obj, obj, sharedId, &sharedVal)) {
      return false;
    }

    // shared's default value is false, which is already the value set above.
    if (!sharedVal.isUndefined()) {
      limits->shared =
          ToBoolean(sharedVal) ? Shareable::True : Shareable::False;

      if (limits->shared == Shareable::True) {
        if (maxVal.isUndefined()) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_WASM_MISSING_MAXIMUM, kind);
          return false;
        }

        if (!cx->realm()
                 ->creationOptions()
                 .getSharedMemoryAndAtomicsEnabled()) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_WASM_NO_SHMEM_LINK);
          return false;
        }
      }
    }
  }

  return true;
}

template <class Class, const char* name>
static JSObject* CreateWasmConstructor(JSContext* cx, JSProtoKey key) {
  RootedAtom className(cx, Atomize(cx, name, strlen(name)));
  if (!className) {
    return nullptr;
  }

  return NewNativeConstructor(cx, Class::construct, 1, className);
}

// ============================================================================
// WebAssembly.Module class and methods

const JSClassOps WasmModuleObject::classOps_ = {
    nullptr,                     // addProperty
    nullptr,                     // delProperty
    nullptr,                     // enumerate
    nullptr,                     // newEnumerate
    nullptr,                     // resolve
    nullptr,                     // mayResolve
    WasmModuleObject::finalize,  // finalize
    nullptr,                     // call
    nullptr,                     // hasInstance
    nullptr,                     // construct
    nullptr,                     // trace
};

const JSClass WasmModuleObject::class_ = {
    "WebAssembly.Module",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmModuleObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmModuleObject::classOps_,
    &WasmModuleObject::classSpec_,
};

const JSClass& WasmModuleObject::protoClass_ = PlainObject::class_;

static constexpr char WasmModuleName[] = "Module";

const ClassSpec WasmModuleObject::classSpec_ = {
    CreateWasmConstructor<WasmModuleObject, WasmModuleName>,
    GenericCreatePrototype<WasmModuleObject>,
    WasmModuleObject::static_methods,
    nullptr,
    WasmModuleObject::methods,
    WasmModuleObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

const JSPropertySpec WasmModuleObject::properties[] = {
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Module", JSPROP_READONLY),
    JS_PS_END};

const JSFunctionSpec WasmModuleObject::methods[] = {JS_FS_END};

const JSFunctionSpec WasmModuleObject::static_methods[] = {
    JS_FN("imports", WasmModuleObject::imports, 1, JSPROP_ENUMERATE),
    JS_FN("exports", WasmModuleObject::exports, 1, JSPROP_ENUMERATE),
    JS_FN("customSections", WasmModuleObject::customSections, 2,
          JSPROP_ENUMERATE),
    JS_FS_END};

/* static */
void WasmModuleObject::finalize(JSFreeOp* fop, JSObject* obj) {
  const Module& module = obj->as<WasmModuleObject>().module();
  obj->zone()->decJitMemory(module.codeLength(module.code().stableTier()));
  fop->release(obj, &module, module.gcMallocBytesExcludingCode(),
               MemoryUse::WasmModule);
}

static bool IsModuleObject(JSObject* obj, const Module** module) {
  WasmModuleObject* mobj = obj->maybeUnwrapIf<WasmModuleObject>();
  if (!mobj) {
    return false;
  }

  *module = &mobj->module();
  return true;
}

static bool GetModuleArg(JSContext* cx, CallArgs args, uint32_t numRequired,
                         const char* name, const Module** module) {
  if (!args.requireAtLeast(cx, name, numRequired)) {
    return false;
  }

  if (!args[0].isObject() || !IsModuleObject(&args[0].toObject(), module)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_MOD_ARG);
    return false;
  }

  return true;
}

struct KindNames {
  RootedPropertyName kind;
  RootedPropertyName table;
  RootedPropertyName memory;
  RootedPropertyName signature;

  explicit KindNames(JSContext* cx)
      : kind(cx), table(cx), memory(cx), signature(cx) {}
};

static bool InitKindNames(JSContext* cx, KindNames* names) {
  JSAtom* kind = Atomize(cx, "kind", strlen("kind"));
  if (!kind) {
    return false;
  }
  names->kind = kind->asPropertyName();

  JSAtom* table = Atomize(cx, "table", strlen("table"));
  if (!table) {
    return false;
  }
  names->table = table->asPropertyName();

  JSAtom* memory = Atomize(cx, "memory", strlen("memory"));
  if (!memory) {
    return false;
  }
  names->memory = memory->asPropertyName();

  JSAtom* signature = Atomize(cx, "signature", strlen("signature"));
  if (!signature) {
    return false;
  }
  names->signature = signature->asPropertyName();

  return true;
}

static JSString* KindToString(JSContext* cx, const KindNames& names,
                              DefinitionKind kind) {
  switch (kind) {
    case DefinitionKind::Function:
      return cx->names().function;
    case DefinitionKind::Table:
      return names.table;
    case DefinitionKind::Memory:
      return names.memory;
    case DefinitionKind::Global:
      return cx->names().global;
  }

  MOZ_CRASH("invalid kind");
}

static JSString* FuncTypeToString(JSContext* cx, const FuncType& funcType) {
  JSStringBuilder buf(cx);
  if (!buf.append('(')) {
    return nullptr;
  }

  bool first = true;
  for (ValType arg : funcType.args()) {
    if (!first && !buf.append(", ", strlen(", "))) {
      return nullptr;
    }

    const char* argStr = ToCString(arg);
    if (!buf.append(argStr, strlen(argStr))) {
      return nullptr;
    }

    first = false;
  }

  if (!buf.append(") -> (", strlen(") -> ("))) {
    return nullptr;
  }

  first = true;
  for (ValType result : funcType.results()) {
    if (!first && !buf.append(", ", strlen(", "))) {
      return nullptr;
    }

    const char* resultStr = ToCString(result);
    if (!buf.append(resultStr, strlen(resultStr))) {
      return nullptr;
    }

    first = false;
  }

  if (!buf.append(')')) {
    return nullptr;
  }

  return buf.finishString();
}

static JSString* UTF8CharsToString(JSContext* cx, const char* chars) {
  return NewStringCopyUTF8Z<CanGC>(cx,
                                   JS::ConstUTF8CharsZ(chars, strlen(chars)));
}

/* static */
bool WasmModuleObject::imports(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  const Module* module;
  if (!GetModuleArg(cx, args, 1, "WebAssembly.Module.imports", &module)) {
    return false;
  }

  KindNames names(cx);
  if (!InitKindNames(cx, &names)) {
    return false;
  }

  RootedValueVector elems(cx);
  if (!elems.reserve(module->imports().length())) {
    return false;
  }

  const FuncImportVector& funcImports =
      module->metadata(module->code().stableTier()).funcImports;

  size_t numFuncImport = 0;
  for (const Import& import : module->imports()) {
    Rooted<IdValueVector> props(cx, IdValueVector(cx));
    if (!props.reserve(3)) {
      return false;
    }

    JSString* moduleStr = UTF8CharsToString(cx, import.module.get());
    if (!moduleStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(cx->names().module), StringValue(moduleStr)));

    JSString* nameStr = UTF8CharsToString(cx, import.field.get());
    if (!nameStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(cx->names().name), StringValue(nameStr)));

    JSString* kindStr = KindToString(cx, names, import.kind);
    if (!kindStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(names.kind), StringValue(kindStr)));

    if (fuzzingSafe && import.kind == DefinitionKind::Function) {
      JSString* ftStr =
          FuncTypeToString(cx, funcImports[numFuncImport++].funcType());
      if (!ftStr) {
        return false;
      }
      if (!props.append(
              IdValuePair(NameToId(names.signature), StringValue(ftStr)))) {
        return false;
      }
    }

    JSObject* obj = ObjectGroup::newPlainObject(cx, props.begin(),
                                                props.length(), GenericObject);
    if (!obj) {
      return false;
    }

    elems.infallibleAppend(ObjectValue(*obj));
  }

  JSObject* arr = NewDenseCopiedArray(cx, elems.length(), elems.begin());
  if (!arr) {
    return false;
  }

  args.rval().setObject(*arr);
  return true;
}

/* static */
bool WasmModuleObject::exports(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  const Module* module;
  if (!GetModuleArg(cx, args, 1, "WebAssembly.Module.exports", &module)) {
    return false;
  }

  KindNames names(cx);
  if (!InitKindNames(cx, &names)) {
    return false;
  }

  RootedValueVector elems(cx);
  if (!elems.reserve(module->exports().length())) {
    return false;
  }

  const FuncExportVector& funcExports =
      module->metadata(module->code().stableTier()).funcExports;

  size_t numFuncExport = 0;
  for (const Export& exp : module->exports()) {
    Rooted<IdValueVector> props(cx, IdValueVector(cx));
    if (!props.reserve(2)) {
      return false;
    }

    JSString* nameStr = UTF8CharsToString(cx, exp.fieldName());
    if (!nameStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(cx->names().name), StringValue(nameStr)));

    JSString* kindStr = KindToString(cx, names, exp.kind());
    if (!kindStr) {
      return false;
    }
    props.infallibleAppend(
        IdValuePair(NameToId(names.kind), StringValue(kindStr)));

    if (fuzzingSafe && exp.kind() == DefinitionKind::Function) {
      JSString* ftStr =
          FuncTypeToString(cx, funcExports[numFuncExport++].funcType());
      if (!ftStr) {
        return false;
      }
      if (!props.append(
              IdValuePair(NameToId(names.signature), StringValue(ftStr)))) {
        return false;
      }
    }

    JSObject* obj = ObjectGroup::newPlainObject(cx, props.begin(),
                                                props.length(), GenericObject);
    if (!obj) {
      return false;
    }

    elems.infallibleAppend(ObjectValue(*obj));
  }

  JSObject* arr = NewDenseCopiedArray(cx, elems.length(), elems.begin());
  if (!arr) {
    return false;
  }

  args.rval().setObject(*arr);
  return true;
}

/* static */
bool WasmModuleObject::customSections(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  const Module* module;
  if (!GetModuleArg(cx, args, 2, "WebAssembly.Module.customSections",
                    &module)) {
    return false;
  }

  Vector<char, 8> name(cx);
  {
    RootedString str(cx, ToString(cx, args.get(1)));
    if (!str) {
      return false;
    }

    Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
    if (!linear) {
      return false;
    }

    if (!name.initLengthUninitialized(
            JS::GetDeflatedUTF8StringLength(linear))) {
      return false;
    }

    mozilla::Unused << JS::DeflateStringToUTF8Buffer(
        linear, MakeSpan(name.begin(), name.length()));
  }

  RootedValueVector elems(cx);
  RootedArrayBufferObject buf(cx);
  for (const CustomSection& cs : module->customSections()) {
    if (name.length() != cs.name.length()) {
      continue;
    }
    if (memcmp(name.begin(), cs.name.begin(), name.length())) {
      continue;
    }

    buf = ArrayBufferObject::createZeroed(cx, cs.payload->length());
    if (!buf) {
      return false;
    }

    memcpy(buf->dataPointer(), cs.payload->begin(), cs.payload->length());
    if (!elems.append(ObjectValue(*buf))) {
      return false;
    }
  }

  JSObject* arr = NewDenseCopiedArray(cx, elems.length(), elems.begin());
  if (!arr) {
    return false;
  }

  args.rval().setObject(*arr);
  return true;
}

/* static */
WasmModuleObject* WasmModuleObject::create(JSContext* cx, const Module& module,
                                           HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  auto* obj = NewObjectWithGivenProto<WasmModuleObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  // This accounts for module allocation size (excluding code which is handled
  // separately - see below). This assumes that the size of associated data
  // doesn't change for the life of the WasmModuleObject. The size is counted
  // once per WasmModuleObject referencing a Module.
  InitReservedSlot(obj, MODULE_SLOT, const_cast<Module*>(&module),
                   module.gcMallocBytesExcludingCode(), MemoryUse::WasmModule);
  module.AddRef();

  // Bug 1569888: We account for the first tier here; the second tier, if
  // different, also needs to be accounted for.
  cx->zone()->incJitMemory(module.codeLength(module.code().stableTier()));
  return obj;
}

static bool GetBufferSource(JSContext* cx, JSObject* obj, unsigned errorNumber,
                            MutableBytes* bytecode) {
  *bytecode = cx->new_<ShareableBytes>();
  if (!*bytecode) {
    return false;
  }

  JSObject* unwrapped = CheckedUnwrapStatic(obj);

  SharedMem<uint8_t*> dataPointer;
  size_t byteLength;
  if (!unwrapped || !IsBufferSource(unwrapped, &dataPointer, &byteLength)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, errorNumber);
    return false;
  }

  if (!(*bytecode)->append(dataPointer.unwrap(), byteLength)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

static SharedCompileArgs InitCompileArgs(JSContext* cx,
                                         const char* introducer) {
  ScriptedCaller scriptedCaller;
  if (!DescribeScriptedCaller(cx, &scriptedCaller, introducer)) {
    return nullptr;
  }

  return CompileArgs::build(cx, std::move(scriptedCaller));
}

static bool ReportCompileWarnings(JSContext* cx,
                                  const UniqueCharsVector& warnings) {
  // Avoid spamming the console.
  size_t numWarnings = std::min<size_t>(warnings.length(), 3);

  for (size_t i = 0; i < numWarnings; i++) {
    if (!WarnNumberASCII(cx, JSMSG_WASM_COMPILE_WARNING, warnings[i].get())) {
      return false;
    }
  }

  if (warnings.length() > numWarnings) {
    if (!WarnNumberASCII(cx, JSMSG_WASM_COMPILE_WARNING,
                         "other warnings suppressed")) {
      return false;
    }
  }

  return true;
}

/* static */
bool WasmModuleObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs callArgs = CallArgsFromVp(argc, vp);

  Log(cx, "sync new Module() started");

  if (!ThrowIfNotConstructing(cx, callArgs, "Module")) {
    return false;
  }

  if (!callArgs.requireAtLeast(cx, "WebAssembly.Module", 1)) {
    return false;
  }

  if (!callArgs[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_ARG);
    return false;
  }

  MutableBytes bytecode;
  if (!GetBufferSource(cx, &callArgs[0].toObject(), JSMSG_WASM_BAD_BUF_ARG,
                       &bytecode)) {
    return false;
  }

  SharedCompileArgs compileArgs = InitCompileArgs(cx, "WebAssembly.Module");
  if (!compileArgs) {
    return false;
  }

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, *bytecode, &error, &warnings);
  if (!module) {
    if (error) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_COMPILE_ERROR, error.get());
      return false;
    }
    ReportOutOfMemory(cx);
    return false;
  }

  if (!ReportCompileWarnings(cx, warnings)) {
    return false;
  }

  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
  RootedObject moduleObj(cx, WasmModuleObject::create(cx, *module, proto));
  if (!moduleObj) {
    return false;
  }

  Log(cx, "sync new Module() succeded");

  callArgs.rval().setObject(*moduleObj);
  return true;
}

const Module& WasmModuleObject::module() const {
  MOZ_ASSERT(is<WasmModuleObject>());
  return *(const Module*)getReservedSlot(MODULE_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly.Instance class and methods

const JSClassOps WasmInstanceObject::classOps_ = {
    nullptr,                       // addProperty
    nullptr,                       // delProperty
    nullptr,                       // enumerate
    nullptr,                       // newEnumerate
    nullptr,                       // resolve
    nullptr,                       // mayResolve
    WasmInstanceObject::finalize,  // finalize
    nullptr,                       // call
    nullptr,                       // hasInstance
    nullptr,                       // construct
    WasmInstanceObject::trace,     // trace
};

const JSClass WasmInstanceObject::class_ = {
    "WebAssembly.Instance",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmInstanceObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmInstanceObject::classOps_,
    &WasmInstanceObject::classSpec_,
};

const JSClass& WasmInstanceObject::protoClass_ = PlainObject::class_;

static constexpr char WasmInstanceName[] = "Instance";

const ClassSpec WasmInstanceObject::classSpec_ = {
    CreateWasmConstructor<WasmInstanceObject, WasmInstanceName>,
    GenericCreatePrototype<WasmInstanceObject>,
    WasmInstanceObject::static_methods,
    nullptr,
    WasmInstanceObject::methods,
    WasmInstanceObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

static bool IsInstance(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmInstanceObject>();
}

/* static */
bool WasmInstanceObject::exportsGetterImpl(JSContext* cx,
                                           const CallArgs& args) {
  args.rval().setObject(
      args.thisv().toObject().as<WasmInstanceObject>().exportsObj());
  return true;
}

/* static */
bool WasmInstanceObject::exportsGetter(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsInstance, exportsGetterImpl>(cx, args);
}

const JSPropertySpec WasmInstanceObject::properties[] = {
    JS_PSG("exports", WasmInstanceObject::exportsGetter, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Instance", JSPROP_READONLY),
    JS_PS_END};

const JSFunctionSpec WasmInstanceObject::methods[] = {JS_FS_END};

const JSFunctionSpec WasmInstanceObject::static_methods[] = {JS_FS_END};

bool WasmInstanceObject::isNewborn() const {
  MOZ_ASSERT(is<WasmInstanceObject>());
  return getReservedSlot(INSTANCE_SLOT).isUndefined();
}

/* static */
void WasmInstanceObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmInstanceObject& instance = obj->as<WasmInstanceObject>();
  fop->delete_(obj, &instance.exports(), MemoryUse::WasmInstanceExports);
  fop->delete_(obj, &instance.scopes(), MemoryUse::WasmInstanceScopes);
  fop->delete_(obj, &instance.indirectGlobals(),
               MemoryUse::WasmInstanceGlobals);
  if (!instance.isNewborn()) {
    if (instance.instance().debugEnabled()) {
      instance.instance().debug().finalize(fop);
    }
    fop->delete_(obj, &instance.instance(), MemoryUse::WasmInstanceInstance);
  }
}

/* static */
void WasmInstanceObject::trace(JSTracer* trc, JSObject* obj) {
  WasmInstanceObject& instanceObj = obj->as<WasmInstanceObject>();
  instanceObj.exports().trace(trc);
  instanceObj.indirectGlobals().trace(trc);
  if (!instanceObj.isNewborn()) {
    instanceObj.instance().tracePrivate(trc);
  }
}

/* static */
WasmInstanceObject* WasmInstanceObject::create(
    JSContext* cx, SharedCode code, const DataSegmentVector& dataSegments,
    const ElemSegmentVector& elemSegments, UniqueTlsData tlsData,
    HandleWasmMemoryObject memory, SharedTableVector&& tables,
    StructTypeDescrVector&& structTypeDescrs,
    const JSFunctionVector& funcImports, const GlobalDescVector& globals,
    const ValVector& globalImportValues,
    const WasmGlobalObjectVector& globalObjs, HandleObject proto,
    UniqueDebugState maybeDebug) {
  UniquePtr<ExportMap> exports = js::MakeUnique<ExportMap>(cx->zone());
  if (!exports) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  UniquePtr<ScopeMap> scopes = js::MakeUnique<ScopeMap>(cx->zone(), cx->zone());
  if (!scopes) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  uint32_t indirectGlobals = 0;

  for (uint32_t i = 0; i < globalObjs.length(); i++) {
    if (globalObjs[i] && globals[i].isIndirect()) {
      indirectGlobals++;
    }
  }

  Rooted<UniquePtr<GlobalObjectVector>> indirectGlobalObjs(
      cx, js::MakeUnique<GlobalObjectVector>(cx->zone()));
  if (!indirectGlobalObjs || !indirectGlobalObjs->resize(indirectGlobals)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  {
    uint32_t next = 0;
    for (uint32_t i = 0; i < globalObjs.length(); i++) {
      if (globalObjs[i] && globals[i].isIndirect()) {
        (*indirectGlobalObjs)[next++] = globalObjs[i];
      }
    }
  }

  Instance* instance = nullptr;
  RootedWasmInstanceObject obj(cx);

  {
    // We must delay creating metadata for this object until after all its
    // slots have been initialized. We must also create the metadata before
    // calling Instance::init as that may allocate new objects.
    AutoSetNewObjectMetadata metadata(cx);
    obj = NewObjectWithGivenProto<WasmInstanceObject>(cx, proto);
    if (!obj) {
      return nullptr;
    }

    MOZ_ASSERT(obj->isTenured(), "assumed by WasmTableObject write barriers");

    // Finalization assumes these slots are always initialized:
    InitReservedSlot(obj, EXPORTS_SLOT, exports.release(),
                     MemoryUse::WasmInstanceExports);

    InitReservedSlot(obj, SCOPES_SLOT, scopes.release(),
                     MemoryUse::WasmInstanceScopes);

    InitReservedSlot(obj, GLOBALS_SLOT, indirectGlobalObjs.release(),
                     MemoryUse::WasmInstanceGlobals);

    obj->initReservedSlot(INSTANCE_SCOPE_SLOT, UndefinedValue());

    // The INSTANCE_SLOT may not be initialized if Instance allocation fails,
    // leading to an observable "newborn" state in tracing/finalization.
    MOZ_ASSERT(obj->isNewborn());

    // Root the Instance via WasmInstanceObject before any possible GC.
    instance = cx->new_<Instance>(
        cx, obj, code, std::move(tlsData), memory, std::move(tables),
        std::move(structTypeDescrs), std::move(maybeDebug));
    if (!instance) {
      return nullptr;
    }

    InitReservedSlot(obj, INSTANCE_SLOT, instance,
                     MemoryUse::WasmInstanceInstance);
    MOZ_ASSERT(!obj->isNewborn());
  }

  if (!instance->init(cx, funcImports, globalImportValues, globalObjs,
                      dataSegments, elemSegments)) {
    return nullptr;
  }

  return obj;
}

void WasmInstanceObject::initExportsObj(JSObject& exportsObj) {
  MOZ_ASSERT(getReservedSlot(EXPORTS_OBJ_SLOT).isUndefined());
  setReservedSlot(EXPORTS_OBJ_SLOT, ObjectValue(exportsObj));
}

static bool GetImportArg(JSContext* cx, CallArgs callArgs,
                         MutableHandleObject importObj) {
  if (!callArgs.get(1).isUndefined()) {
    if (!callArgs[1].isObject()) {
      return ThrowBadImportArg(cx);
    }
    importObj.set(&callArgs[1].toObject());
  }
  return true;
}

/* static */
bool WasmInstanceObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Log(cx, "sync new Instance() started");

  if (!ThrowIfNotConstructing(cx, args, "Instance")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Instance", 1)) {
    return false;
  }

  const Module* module;
  if (!args[0].isObject() || !IsModuleObject(&args[0].toObject(), &module)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_MOD_ARG);
    return false;
  }

  RootedObject importObj(cx);
  if (!GetImportArg(cx, args, &importObj)) {
    return false;
  }

  RootedObject instanceProto(
      cx, &cx->global()->getPrototype(JSProto_WasmInstance).toObject());

  Rooted<ImportValues> imports(cx);
  if (!GetImports(cx, *module, importObj, imports.address())) {
    return false;
  }

  RootedWasmInstanceObject instanceObj(cx);
  if (!module->instantiate(cx, imports.get(), instanceProto, &instanceObj)) {
    return false;
  }

  Log(cx, "sync new Instance() succeeded");

  args.rval().setObject(*instanceObj);
  return true;
}

Instance& WasmInstanceObject::instance() const {
  MOZ_ASSERT(!isNewborn());
  return *(Instance*)getReservedSlot(INSTANCE_SLOT).toPrivate();
}

JSObject& WasmInstanceObject::exportsObj() const {
  return getReservedSlot(EXPORTS_OBJ_SLOT).toObject();
}

WasmInstanceObject::ExportMap& WasmInstanceObject::exports() const {
  return *(ExportMap*)getReservedSlot(EXPORTS_SLOT).toPrivate();
}

WasmInstanceObject::ScopeMap& WasmInstanceObject::scopes() const {
  return *(ScopeMap*)getReservedSlot(SCOPES_SLOT).toPrivate();
}

WasmInstanceObject::GlobalObjectVector& WasmInstanceObject::indirectGlobals()
    const {
  return *(GlobalObjectVector*)getReservedSlot(GLOBALS_SLOT).toPrivate();
}

static bool WasmCall(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedFunction callee(cx, &args.callee().as<JSFunction>());

  Instance& instance = ExportedFunctionToInstance(callee);
  uint32_t funcIndex = ExportedFunctionToFuncIndex(callee);
  return instance.callExport(cx, funcIndex, args);
}

/* static */
bool WasmInstanceObject::getExportedFunction(
    JSContext* cx, HandleWasmInstanceObject instanceObj, uint32_t funcIndex,
    MutableHandleFunction fun) {
  if (ExportMap::Ptr p = instanceObj->exports().lookup(funcIndex)) {
    fun.set(p->value());
    return true;
  }

  const Instance& instance = instanceObj->instance();
  const FuncExport& funcExport =
      instance.metadata(instance.code().bestTier()).lookupFuncExport(funcIndex);
  unsigned numArgs = funcExport.funcType().args().length();

  if (instance.isAsmJS()) {
    // asm.js needs to act like a normal JS function which means having the
    // name from the original source and being callable as a constructor.
    RootedAtom name(cx, instance.getFuncDisplayAtom(cx, funcIndex));
    if (!name) {
      return false;
    }
    fun.set(NewNativeConstructor(cx, WasmCall, numArgs, name,
                                 gc::AllocKind::FUNCTION_EXTENDED,
                                 SingletonObject, FunctionFlags::ASMJS_CTOR));
    if (!fun) {
      return false;
    }

    // asm.js does not support jit entries.
    fun->setWasmFuncIndex(funcIndex);
  } else {
    RootedAtom name(cx, NumberToAtom(cx, funcIndex));
    if (!name) {
      return false;
    }

    fun.set(NewNativeFunction(cx, WasmCall, numArgs, name,
                              gc::AllocKind::FUNCTION_EXTENDED, SingletonObject,
                              FunctionFlags::WASM));
    if (!fun) {
      return false;
    }

    // Some applications eagerly access all table elements which currently
    // triggers worst-case behavior for lazy stubs, since each will allocate a
    // separate 4kb code page. Most eagerly-accessed functions are not called,
    // so instead wait until Instance::callExport() to create the entry stubs.
    if (funcExport.hasEagerStubs() && funcExport.canHaveJitEntry()) {
      fun->setWasmJitEntry(instance.code().getAddressOfJitEntry(funcIndex));
    } else {
      fun->setWasmFuncIndex(funcIndex);
    }
  }

  fun->setExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT,
                       ObjectValue(*instanceObj));

  void* tlsData = instanceObj->instance().tlsData();
  fun->setExtendedSlot(FunctionExtended::WASM_TLSDATA_SLOT,
                       PrivateValue(tlsData));

  if (!instanceObj->exports().putNew(funcIndex, fun)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

const CodeRange& WasmInstanceObject::getExportedFunctionCodeRange(
    JSFunction* fun, Tier tier) {
  uint32_t funcIndex = ExportedFunctionToFuncIndex(fun);
  MOZ_ASSERT(exports().lookup(funcIndex)->value() == fun);
  const MetadataTier& metadata = instance().metadata(tier);
  return metadata.codeRange(metadata.lookupFuncExport(funcIndex));
}

/* static */
WasmInstanceScope* WasmInstanceObject::getScope(
    JSContext* cx, HandleWasmInstanceObject instanceObj) {
  if (!instanceObj->getReservedSlot(INSTANCE_SCOPE_SLOT).isUndefined()) {
    return (WasmInstanceScope*)instanceObj->getReservedSlot(INSTANCE_SCOPE_SLOT)
        .toGCThing();
  }

  Rooted<WasmInstanceScope*> instanceScope(
      cx, WasmInstanceScope::create(cx, instanceObj));
  if (!instanceScope) {
    return nullptr;
  }

  instanceObj->setReservedSlot(INSTANCE_SCOPE_SLOT,
                               PrivateGCThingValue(instanceScope));

  return instanceScope;
}

/* static */
WasmFunctionScope* WasmInstanceObject::getFunctionScope(
    JSContext* cx, HandleWasmInstanceObject instanceObj, uint32_t funcIndex) {
  if (ScopeMap::Ptr p = instanceObj->scopes().lookup(funcIndex)) {
    return p->value();
  }

  Rooted<WasmInstanceScope*> instanceScope(
      cx, WasmInstanceObject::getScope(cx, instanceObj));
  if (!instanceScope) {
    return nullptr;
  }

  Rooted<WasmFunctionScope*> funcScope(
      cx, WasmFunctionScope::create(cx, instanceScope, funcIndex));
  if (!funcScope) {
    return nullptr;
  }

  if (!instanceObj->scopes().putNew(funcIndex, funcScope)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return funcScope;
}

bool wasm::IsWasmExportedFunction(JSFunction* fun) {
  return fun->kind() == FunctionFlags::Wasm;
}

bool wasm::CheckFuncRefValue(JSContext* cx, HandleValue v,
                             MutableHandleFunction fun) {
  if (v.isNull()) {
    MOZ_ASSERT(!fun);
    return true;
  }

  if (v.isObject()) {
    JSObject& obj = v.toObject();
    if (obj.is<JSFunction>()) {
      JSFunction* f = &obj.as<JSFunction>();
      if (IsWasmExportedFunction(f)) {
        fun.set(f);
        return true;
      }
    }
  }

  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_FUNCREF_VALUE);
  return false;
}

Instance& wasm::ExportedFunctionToInstance(JSFunction* fun) {
  return ExportedFunctionToInstanceObject(fun)->instance();
}

WasmInstanceObject* wasm::ExportedFunctionToInstanceObject(JSFunction* fun) {
  MOZ_ASSERT(fun->kind() == FunctionFlags::Wasm ||
             fun->kind() == FunctionFlags::AsmJS);
  const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT);
  return &v.toObject().as<WasmInstanceObject>();
}

uint32_t wasm::ExportedFunctionToFuncIndex(JSFunction* fun) {
  Instance& instance = ExportedFunctionToInstanceObject(fun)->instance();
  return instance.code().getFuncIndex(fun);
}

// ============================================================================
// WebAssembly.Memory class and methods

const JSClassOps WasmMemoryObject::classOps_ = {
    nullptr,                     // addProperty
    nullptr,                     // delProperty
    nullptr,                     // enumerate
    nullptr,                     // newEnumerate
    nullptr,                     // resolve
    nullptr,                     // mayResolve
    WasmMemoryObject::finalize,  // finalize
    nullptr,                     // call
    nullptr,                     // hasInstance
    nullptr,                     // construct
    nullptr,                     // trace
};

const JSClass WasmMemoryObject::class_ = {
    "WebAssembly.Memory",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmMemoryObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmMemoryObject::classOps_, &WasmMemoryObject::classSpec_};

const JSClass& WasmMemoryObject::protoClass_ = PlainObject::class_;

static constexpr char WasmMemoryName[] = "Memory";

const ClassSpec WasmMemoryObject::classSpec_ = {
    CreateWasmConstructor<WasmMemoryObject, WasmMemoryName>,
    GenericCreatePrototype<WasmMemoryObject>,
    WasmMemoryObject::static_methods,
    nullptr,
    WasmMemoryObject::methods,
    WasmMemoryObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

/* static */
void WasmMemoryObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmMemoryObject& memory = obj->as<WasmMemoryObject>();
  if (memory.hasObservers()) {
    fop->delete_(obj, &memory.observers(), MemoryUse::WasmMemoryObservers);
  }
}

/* static */
WasmMemoryObject* WasmMemoryObject::create(
    JSContext* cx, HandleArrayBufferObjectMaybeShared buffer,
    HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  auto* obj = NewObjectWithGivenProto<WasmMemoryObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  obj->initReservedSlot(BUFFER_SLOT, ObjectValue(*buffer));
  MOZ_ASSERT(!obj->hasObservers());
  return obj;
}

/* static */
bool WasmMemoryObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Memory")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Memory", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "memory");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  Limits limits;
  if (!GetLimits(cx, obj, MaxMemoryInitialPages, MaxMemoryMaximumPages,
                 "Memory", &limits, Shareable::True)) {
    return false;
  }

  ConvertMemoryPagesToBytes(&limits);

  RootedArrayBufferObjectMaybeShared buffer(cx);
  if (!CreateWasmBuffer(cx, limits, &buffer)) {
    return false;
  }

  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());
  RootedWasmMemoryObject memoryObj(cx,
                                   WasmMemoryObject::create(cx, buffer, proto));
  if (!memoryObj) {
    return false;
  }

  args.rval().setObject(*memoryObj);
  return true;
}

static bool IsMemory(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmMemoryObject>();
}

/* static */
bool WasmMemoryObject::bufferGetterImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmMemoryObject memoryObj(
      cx, &args.thisv().toObject().as<WasmMemoryObject>());
  RootedArrayBufferObjectMaybeShared buffer(cx, &memoryObj->buffer());

  if (memoryObj->isShared()) {
    uint32_t memoryLength = memoryObj->volatileMemoryLength();
    MOZ_ASSERT(memoryLength >= buffer->byteLength());

    if (memoryLength > buffer->byteLength()) {
      RootedSharedArrayBufferObject newBuffer(
          cx, SharedArrayBufferObject::New(
                  cx, memoryObj->sharedArrayRawBuffer(), memoryLength));
      if (!newBuffer) {
        return false;
      }
      // OK to addReference after we try to allocate because the memoryObj
      // keeps the rawBuffer alive.
      if (!memoryObj->sharedArrayRawBuffer()->addReference()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_SC_SAB_REFCNT_OFLO);
        return false;
      }
      buffer = newBuffer;
      memoryObj->setReservedSlot(BUFFER_SLOT, ObjectValue(*newBuffer));
    }
  }

  args.rval().setObject(*buffer);
  return true;
}

/* static */
bool WasmMemoryObject::bufferGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsMemory, bufferGetterImpl>(cx, args);
}

const JSPropertySpec WasmMemoryObject::properties[] = {
    JS_PSG("buffer", WasmMemoryObject::bufferGetter, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Memory", JSPROP_READONLY),
    JS_PS_END};

/* static */
bool WasmMemoryObject::growImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmMemoryObject memory(
      cx, &args.thisv().toObject().as<WasmMemoryObject>());

  if (!args.requireAtLeast(cx, "WebAssembly.Memory.grow", 1)) {
    return false;
  }

  uint32_t delta;
  if (!EnforceRangeU32(cx, args.get(0), "Memory", "grow delta", &delta)) {
    return false;
  }

  uint32_t ret = grow(memory, delta, cx);

  if (ret == uint32_t(-1)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_GROW,
                             "memory");
    return false;
  }

  args.rval().setInt32(ret);
  return true;
}

/* static */
bool WasmMemoryObject::grow(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsMemory, growImpl>(cx, args);
}

const JSFunctionSpec WasmMemoryObject::methods[] = {
    JS_FN("grow", WasmMemoryObject::grow, 1, JSPROP_ENUMERATE), JS_FS_END};

const JSFunctionSpec WasmMemoryObject::static_methods[] = {JS_FS_END};

ArrayBufferObjectMaybeShared& WasmMemoryObject::buffer() const {
  return getReservedSlot(BUFFER_SLOT)
      .toObject()
      .as<ArrayBufferObjectMaybeShared>();
}

SharedArrayRawBuffer* WasmMemoryObject::sharedArrayRawBuffer() const {
  MOZ_ASSERT(isShared());
  return buffer().as<SharedArrayBufferObject>().rawBufferObject();
}

uint32_t WasmMemoryObject::volatileMemoryLength() const {
  if (isShared()) {
    return sharedArrayRawBuffer()->volatileByteLength();
  }
  return buffer().byteLength();
}

bool WasmMemoryObject::isShared() const {
  return buffer().is<SharedArrayBufferObject>();
}

bool WasmMemoryObject::hasObservers() const {
  return !getReservedSlot(OBSERVERS_SLOT).isUndefined();
}

WasmMemoryObject::InstanceSet& WasmMemoryObject::observers() const {
  MOZ_ASSERT(hasObservers());
  return *reinterpret_cast<InstanceSet*>(
      getReservedSlot(OBSERVERS_SLOT).toPrivate());
}

WasmMemoryObject::InstanceSet* WasmMemoryObject::getOrCreateObservers(
    JSContext* cx) {
  if (!hasObservers()) {
    auto observers = MakeUnique<InstanceSet>(cx->zone(), cx->zone());
    if (!observers) {
      ReportOutOfMemory(cx);
      return nullptr;
    }

    InitReservedSlot(this, OBSERVERS_SLOT, observers.release(),
                     MemoryUse::WasmMemoryObservers);
  }

  return &observers();
}

bool WasmMemoryObject::isHuge() const {
#ifdef WASM_SUPPORTS_HUGE_MEMORY
  static_assert(ArrayBufferObject::MaxBufferByteLength < HugeMappedSize,
                "Non-huge buffer may be confused as huge");
  return buffer().wasmMappedSize() >= HugeMappedSize;
#else
  return false;
#endif
}

bool WasmMemoryObject::movingGrowable() const {
  return !isHuge() && !buffer().wasmMaxSize();
}

uint32_t WasmMemoryObject::boundsCheckLimit() const {
  if (!buffer().isWasm() || isHuge()) {
    return buffer().byteLength();
  }
  size_t mappedSize = buffer().wasmMappedSize();
  MOZ_ASSERT(mappedSize <= UINT32_MAX);
  MOZ_ASSERT(mappedSize >= wasm::GuardSize);
  MOZ_ASSERT(wasm::IsValidBoundsCheckImmediate(mappedSize - wasm::GuardSize));
  return mappedSize - wasm::GuardSize;
}

bool WasmMemoryObject::addMovingGrowObserver(JSContext* cx,
                                             WasmInstanceObject* instance) {
  MOZ_ASSERT(movingGrowable());

  InstanceSet* observers = getOrCreateObservers(cx);
  if (!observers) {
    return false;
  }

  if (!observers->putNew(instance)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/* static */
uint32_t WasmMemoryObject::growShared(HandleWasmMemoryObject memory,
                                      uint32_t delta) {
  SharedArrayRawBuffer* rawBuf = memory->sharedArrayRawBuffer();
  SharedArrayRawBuffer::Lock lock(rawBuf);

  MOZ_ASSERT(rawBuf->volatileByteLength() % PageSize == 0);
  uint32_t oldNumPages = rawBuf->volatileByteLength() / PageSize;

  CheckedInt<uint32_t> newSize = oldNumPages;
  newSize += delta;
  newSize *= PageSize;
  if (!newSize.isValid()) {
    return -1;
  }

  if (newSize.value() > rawBuf->maxSize()) {
    return -1;
  }

  if (!rawBuf->wasmGrowToSizeInPlace(lock, newSize.value())) {
    return -1;
  }

  // New buffer objects will be created lazily in all agents (including in
  // this agent) by bufferGetterImpl, above, so no more work to do here.

  return oldNumPages;
}

/* static */
uint32_t WasmMemoryObject::grow(HandleWasmMemoryObject memory, uint32_t delta,
                                JSContext* cx) {
  if (memory->isShared()) {
    return growShared(memory, delta);
  }

  RootedArrayBufferObject oldBuf(cx, &memory->buffer().as<ArrayBufferObject>());

  MOZ_ASSERT(oldBuf->byteLength() % PageSize == 0);
  uint32_t oldNumPages = oldBuf->byteLength() / PageSize;

  CheckedInt<uint32_t> newSize = oldNumPages;
  newSize += delta;
  newSize *= PageSize;
  if (!newSize.isValid()) {
    return -1;
  }

  RootedArrayBufferObject newBuf(cx);

  if (memory->movingGrowable()) {
    MOZ_ASSERT(!memory->isHuge());
    if (!ArrayBufferObject::wasmMovingGrowToSize(newSize.value(), oldBuf,
                                                 &newBuf, cx)) {
      return -1;
    }
  } else {
    if (Maybe<uint32_t> maxSize = oldBuf->wasmMaxSize()) {
      if (newSize.value() > maxSize.value()) {
        return -1;
      }
    }

    if (!ArrayBufferObject::wasmGrowToSizeInPlace(newSize.value(), oldBuf,
                                                  &newBuf, cx)) {
      return -1;
    }
  }

  memory->setReservedSlot(BUFFER_SLOT, ObjectValue(*newBuf));

  // Only notify moving-grow-observers after the BUFFER_SLOT has been updated
  // since observers will call buffer().
  if (memory->hasObservers()) {
    for (InstanceSet::Range r = memory->observers().all(); !r.empty();
         r.popFront()) {
      r.front()->instance().onMovingGrowMemory();
    }
  }

  return oldNumPages;
}

bool js::wasm::IsSharedWasmMemoryObject(JSObject* obj) {
  WasmMemoryObject* mobj = obj->maybeUnwrapIf<WasmMemoryObject>();
  return mobj && mobj->isShared();
}

// ============================================================================
// WebAssembly.Table class and methods

const JSClassOps WasmTableObject::classOps_ = {
    nullptr,                    // addProperty
    nullptr,                    // delProperty
    nullptr,                    // enumerate
    nullptr,                    // newEnumerate
    nullptr,                    // resolve
    nullptr,                    // mayResolve
    WasmTableObject::finalize,  // finalize
    nullptr,                    // call
    nullptr,                    // hasInstance
    nullptr,                    // construct
    WasmTableObject::trace,     // trace
};

const JSClass WasmTableObject::class_ = {
    "WebAssembly.Table",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(WasmTableObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmTableObject::classOps_, &WasmTableObject::classSpec_};

const JSClass& WasmTableObject::protoClass_ = PlainObject::class_;

static constexpr char WasmTableName[] = "Table";

const ClassSpec WasmTableObject::classSpec_ = {
    CreateWasmConstructor<WasmTableObject, WasmTableName>,
    GenericCreatePrototype<WasmTableObject>,
    WasmTableObject::static_methods,
    nullptr,
    WasmTableObject::methods,
    WasmTableObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

bool WasmTableObject::isNewborn() const {
  MOZ_ASSERT(is<WasmTableObject>());
  return getReservedSlot(TABLE_SLOT).isUndefined();
}

/* static */
void WasmTableObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmTableObject& tableObj = obj->as<WasmTableObject>();
  if (!tableObj.isNewborn()) {
    auto& table = tableObj.table();
    fop->release(obj, &table, table.gcMallocBytes(), MemoryUse::WasmTableTable);
  }
}

/* static */
void WasmTableObject::trace(JSTracer* trc, JSObject* obj) {
  WasmTableObject& tableObj = obj->as<WasmTableObject>();
  if (!tableObj.isNewborn()) {
    tableObj.table().tracePrivate(trc);
  }
}

/* static */
WasmTableObject* WasmTableObject::create(JSContext* cx, const Limits& limits,
                                         TableKind tableKind) {
  RootedObject proto(cx,
                     &cx->global()->getPrototype(JSProto_WasmTable).toObject());

  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmTableObject obj(
      cx, NewObjectWithGivenProto<WasmTableObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());

  TableDesc td(tableKind, limits, /*importedOrExported=*/true);

  SharedTable table = Table::create(cx, td, obj);
  if (!table) {
    return nullptr;
  }

  size_t size = table->gcMallocBytes();
  InitReservedSlot(obj, TABLE_SLOT, table.forget().take(), size,
                   MemoryUse::WasmTableTable);

  MOZ_ASSERT(!obj->isNewborn());
  return obj;
}

/* static */
bool WasmTableObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Table")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Table", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "table");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());

  JSAtom* elementAtom = Atomize(cx, "element", strlen("element"));
  if (!elementAtom) {
    return false;
  }
  RootedId elementId(cx, AtomToId(elementAtom));

  RootedValue elementVal(cx);
  if (!GetProperty(cx, obj, obj, elementId, &elementVal)) {
    return false;
  }

  RootedString elementStr(cx, ToString(cx, elementVal));
  if (!elementStr) {
    return false;
  }

  RootedLinearString elementLinearStr(cx, elementStr->ensureLinear(cx));
  if (!elementLinearStr) {
    return false;
  }

  TableKind tableKind;
  if (StringEqualsLiteral(elementLinearStr, "anyfunc") ||
      StringEqualsLiteral(elementLinearStr, "funcref")) {
    tableKind = TableKind::FuncRef;
#ifdef ENABLE_WASM_REFTYPES
  } else if (StringEqualsLiteral(elementLinearStr, "anyref") ||
             StringEqualsLiteral(elementLinearStr, "nullref")) {
    if (!ReftypesAvailable(cx)) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_ELEMENT);
      return false;
    }
    if (StringEqualsLiteral(elementLinearStr, "anyref")) {
      tableKind = TableKind::AnyRef;
    } else {
      tableKind = TableKind::NullRef;
    }
#endif
  } else {
#ifdef ENABLE_WASM_REFTYPES
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_ELEMENT_GENERALIZED);
#else
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_ELEMENT);
#endif
    return false;
  }

  Limits limits;
  if (!GetLimits(cx, obj, MaxTableInitialLength, MaxTableLength, "Table",
                 &limits, Shareable::False)) {
    return false;
  }

  RootedWasmTableObject table(cx,
                              WasmTableObject::create(cx, limits, tableKind));
  if (!table) {
    return false;
  }

  args.rval().setObject(*table);
  return true;
}

static bool IsTable(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmTableObject>();
}

/* static */
bool WasmTableObject::lengthGetterImpl(JSContext* cx, const CallArgs& args) {
  args.rval().setNumber(
      args.thisv().toObject().as<WasmTableObject>().table().length());
  return true;
}

/* static */
bool WasmTableObject::lengthGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, lengthGetterImpl>(cx, args);
}

const JSPropertySpec WasmTableObject::properties[] = {
    JS_PSG("length", WasmTableObject::lengthGetter, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Table", JSPROP_READONLY),
    JS_PS_END};

static bool ToTableIndex(JSContext* cx, HandleValue v, const Table& table,
                         const char* noun, uint32_t* index) {
  if (!EnforceRangeU32(cx, v, "Table", noun, index)) {
    return false;
  }

  if (*index >= table.length()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_BAD_RANGE, "Table", noun);
    return false;
  }

  return true;
}

/* static */
bool WasmTableObject::getImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmTableObject tableObj(
      cx, &args.thisv().toObject().as<WasmTableObject>());
  const Table& table = tableObj->table();

  if (!args.requireAtLeast(cx, "WebAssembly.Table.get", 1)) {
    return false;
  }

  uint32_t index;
  if (!ToTableIndex(cx, args.get(0), table, "get index", &index)) {
    return false;
  }

  switch (table.repr()) {
    case TableRepr::Func: {
      MOZ_RELEASE_ASSERT(table.kind() == TableKind::FuncRef);
      RootedFunction fun(cx);
      if (!table.getFuncRef(cx, index, &fun)) {
        return false;
      }
      args.rval().setObjectOrNull(fun);
      break;
    }
    case TableRepr::Ref: {
      args.rval().set(UnboxAnyRef(table.getAnyRef(index)));
      break;
    }
  }
  return true;
}

/* static */
bool WasmTableObject::get(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, getImpl>(cx, args);
}

/* static */
bool WasmTableObject::setImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmTableObject tableObj(
      cx, &args.thisv().toObject().as<WasmTableObject>());
  Table& table = tableObj->table();

  if (!args.requireAtLeast(cx, "WebAssembly.Table.set", 2)) {
    return false;
  }

  uint32_t index;
  if (!ToTableIndex(cx, args.get(0), table, "set index", &index)) {
    return false;
  }

  MOZ_ASSERT(index < MaxTableLength);
  static_assert(MaxTableLength < UINT32_MAX, "Invariant");

  RootedValue fillValue(cx, args[1]);
  RootedFunction fun(cx);
  RootedAnyRef any(cx, AnyRef::null());
  if (!CheckRefType(cx, ToElemValType(table.kind()).refTypeKind(), fillValue,
                    &fun, &any)) {
    return false;
  }
  switch (table.kind()) {
    case TableKind::AsmJS:
      MOZ_CRASH("Should not happen");
    case TableKind::FuncRef:
      table.fillFuncRef(index, 1, FuncRef::fromJSFunction(fun), cx);
      break;
    case TableKind::NullRef:
    case TableKind::AnyRef:
      table.fillAnyRef(index, 1, any);
      break;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool WasmTableObject::set(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, setImpl>(cx, args);
}

/* static */
bool WasmTableObject::growImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmTableObject tableObj(
      cx, &args.thisv().toObject().as<WasmTableObject>());
  Table& table = tableObj->table();

  if (!args.requireAtLeast(cx, "WebAssembly.Table.grow", 1)) {
    return false;
  }

  uint32_t delta;
  if (!EnforceRangeU32(cx, args.get(0), "Table", "grow delta", &delta)) {
    return false;
  }

  uint32_t oldLength = table.grow(delta);

  if (oldLength == uint32_t(-1)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_GROW,
                             "table");
    return false;
  }

  RootedValue fillValue(cx);
  fillValue.setNull();
  if (args.length() > 1) {
    fillValue = args[1];
  }

  MOZ_ASSERT(delta <= MaxTableLength);              // grow() should ensure this
  MOZ_ASSERT(oldLength <= MaxTableLength - delta);  // ditto

  static_assert(MaxTableLength < UINT32_MAX, "Invariant");

  if (!fillValue.isNull()) {
    RootedFunction fun(cx);
    RootedAnyRef any(cx, AnyRef::null());
    if (!CheckRefType(cx, ToElemValType(table.kind()).refTypeKind(), fillValue,
                      &fun, &any)) {
      return false;
    }
    switch (table.repr()) {
      case TableRepr::Func:
        MOZ_ASSERT(table.kind() == TableKind::FuncRef);
        table.fillFuncRef(oldLength, delta, FuncRef::fromJSFunction(fun), cx);
        break;
      case TableRepr::Ref:
        table.fillAnyRef(oldLength, delta, any);
        break;
    }
  }

#ifdef DEBUG
  if (fillValue.isNull()) {
    switch (table.repr()) {
      case TableRepr::Func:
        for (uint32_t index = oldLength; index < oldLength + delta; index++) {
          MOZ_ASSERT(table.getFuncRef(index).code == nullptr);
        }
        break;
      case TableRepr::Ref:
        for (uint32_t index = oldLength; index < oldLength + delta; index++) {
          MOZ_ASSERT(table.getAnyRef(index).isNull());
        }
        break;
    }
  }
#endif

  args.rval().setInt32(oldLength);
  return true;
}

/* static */
bool WasmTableObject::grow(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, growImpl>(cx, args);
}

const JSFunctionSpec WasmTableObject::methods[] = {
    JS_FN("get", WasmTableObject::get, 1, JSPROP_ENUMERATE),
    JS_FN("set", WasmTableObject::set, 2, JSPROP_ENUMERATE),
    JS_FN("grow", WasmTableObject::grow, 1, JSPROP_ENUMERATE), JS_FS_END};

const JSFunctionSpec WasmTableObject::static_methods[] = {JS_FS_END};

Table& WasmTableObject::table() const {
  return *(Table*)getReservedSlot(TABLE_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly.global class and methods

const JSClassOps WasmGlobalObject::classOps_ = {
    nullptr,                     // addProperty
    nullptr,                     // delProperty
    nullptr,                     // enumerate
    nullptr,                     // newEnumerate
    nullptr,                     // resolve
    nullptr,                     // mayResolve
    WasmGlobalObject::finalize,  // finalize
    nullptr,                     // call
    nullptr,                     // hasInstance
    nullptr,                     // construct
    WasmGlobalObject::trace,     // trace
};

const JSClass WasmGlobalObject::class_ = {
    "WebAssembly.Global",
    JSCLASS_HAS_RESERVED_SLOTS(WasmGlobalObject::RESERVED_SLOTS) |
        JSCLASS_BACKGROUND_FINALIZE,
    &WasmGlobalObject::classOps_, &WasmGlobalObject::classSpec_};

const JSClass& WasmGlobalObject::protoClass_ = PlainObject::class_;

static constexpr char WasmGlobalName[] = "Global";

const ClassSpec WasmGlobalObject::classSpec_ = {
    CreateWasmConstructor<WasmGlobalObject, WasmGlobalName>,
    GenericCreatePrototype<WasmGlobalObject>,
    WasmGlobalObject::static_methods,
    nullptr,
    WasmGlobalObject::methods,
    WasmGlobalObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

/* static */
void WasmGlobalObject::trace(JSTracer* trc, JSObject* obj) {
  WasmGlobalObject* global = reinterpret_cast<WasmGlobalObject*>(obj);
  if (global->isNewborn()) {
    // This can happen while we're allocating the object, in which case
    // every single slot of the object is not defined yet. In particular,
    // there's nothing to trace yet.
    return;
  }
  switch (global->type().kind()) {
    case ValType::I32:
    case ValType::F32:
    case ValType::I64:
    case ValType::F64:
      break;
    case ValType::Ref:
      switch (global->type().refTypeKind()) {
        case RefType::Func:
        case RefType::Any:
          if (!global->cell()->ref.isNull()) {
            // TODO/AnyRef-boxing: With boxed immediates and strings, the write
            // barrier is going to have to be more complicated.
            ASSERT_ANYREF_IS_JSOBJECT;
            TraceManuallyBarrieredEdge(trc,
                                       global->cell()->ref.asJSObjectAddress(),
                                       "wasm reference-typed global");
          }
          break;
        case RefType::Null:
          break;
        case RefType::TypeIndex:
          MOZ_CRASH("Ref NYI");
      }
      break;
  }
}

/* static */
void WasmGlobalObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmGlobalObject* global = reinterpret_cast<WasmGlobalObject*>(obj);
  if (!global->isNewborn()) {
    fop->delete_(obj, global->cell(), MemoryUse::WasmGlobalCell);
  }
}

/* static */
WasmGlobalObject* WasmGlobalObject::create(JSContext* cx, HandleVal hval,
                                           bool isMutable) {
  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmGlobal).toObject());

  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmGlobalObject obj(
      cx, NewObjectWithGivenProto<WasmGlobalObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());
  MOZ_ASSERT(obj->isTenured(), "assumed by global.set post barriers");

  // It's simpler to initialize the cell after the object has been created,
  // to avoid needing to root the cell before the object creation.

  Cell* cell = js_new<Cell>();
  if (!cell) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  const Val& val = hval.get();
  switch (val.type().kind()) {
    case ValType::I32:
      cell->i32 = val.i32();
      break;
    case ValType::I64:
      cell->i64 = val.i64();
      break;
    case ValType::F32:
      cell->f32 = val.f32();
      break;
    case ValType::F64:
      cell->f64 = val.f64();
      break;
    case ValType::Ref:
      switch (val.type().refTypeKind()) {
        case RefType::Func:
        case RefType::Any:
          MOZ_ASSERT(cell->ref.isNull(), "no prebarriers needed");
          cell->ref = val.ref();
          if (!cell->ref.isNull()) {
            // TODO/AnyRef-boxing: With boxed immediates and strings, the write
            // barrier is going to have to be more complicated.
            ASSERT_ANYREF_IS_JSOBJECT;
            JSObject::writeBarrierPost(cell->ref.asJSObjectAddress(), nullptr,
                                       cell->ref.asJSObject());
          }
          break;
        case RefType::Null:
          break;
        case RefType::TypeIndex:
          MOZ_CRASH("Ref NYI");
      }
      break;
  }
  obj->initReservedSlot(TYPE_SLOT,
                        Int32Value(int32_t(val.type().bitsUnsafe())));
  obj->initReservedSlot(MUTABLE_SLOT, JS::BooleanValue(isMutable));
  InitReservedSlot(obj, CELL_SLOT, cell, MemoryUse::WasmGlobalCell);

  MOZ_ASSERT(!obj->isNewborn());

  return obj;
}

/* static */
bool WasmGlobalObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Global")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Global", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "global");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());

  // Extract properties in lexicographic order per spec.

  RootedValue mutableVal(cx);
  if (!JS_GetProperty(cx, obj, "mutable", &mutableVal)) {
    return false;
  }

  RootedValue typeVal(cx);
  if (!JS_GetProperty(cx, obj, "value", &typeVal)) {
    return false;
  }

  RootedString typeStr(cx, ToString(cx, typeVal));
  if (!typeStr) {
    return false;
  }

  RootedLinearString typeLinearStr(cx, typeStr->ensureLinear(cx));
  if (!typeLinearStr) {
    return false;
  }

  ValType globalType;
  if (StringEqualsLiteral(typeLinearStr, "i32")) {
    globalType = ValType::I32;
  } else if (args.length() == 1 && StringEqualsLiteral(typeLinearStr, "i64")) {
    // For the time being, i64 is allowed only if there is not an
    // initializing value.
    globalType = ValType::I64;
  } else if (StringEqualsLiteral(typeLinearStr, "f32")) {
    globalType = ValType::F32;
  } else if (StringEqualsLiteral(typeLinearStr, "f64")) {
    globalType = ValType::F64;
#ifdef ENABLE_WASM_BIGINT
  } else if (I64BigIntConversionAvailable(cx) &&
             StringEqualsLiteral(typeLinearStr, "i64")) {
    globalType = ValType::I64;
#endif
#ifdef ENABLE_WASM_REFTYPES
  } else if (ReftypesAvailable(cx) &&
             StringEqualsLiteral(typeLinearStr, "funcref")) {
    globalType = RefType::func();
  } else if (ReftypesAvailable(cx) &&
             StringEqualsLiteral(typeLinearStr, "anyref")) {
    globalType = RefType::any();
  } else if (ReftypesAvailable(cx) &&
             StringEqualsLiteral(typeLinearStr, "nullref")) {
    globalType = RefType::null();
#endif
  } else {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_GLOBAL_TYPE);
    return false;
  }

  bool isMutable = ToBoolean(mutableVal);

  // Extract the initial value, or provide a suitable default.
  RootedVal globalVal(cx);

  // Initialize with default value.
  switch (globalType.kind()) {
    case ValType::I32:
      globalVal = Val(uint32_t(0));
      break;
    case ValType::I64:
      globalVal = Val(uint64_t(0));
      break;
    case ValType::F32:
      globalVal = Val(float(0.0));
      break;
    case ValType::F64:
      globalVal = Val(double(0.0));
      break;
    case ValType::Ref:
      switch (globalType.refTypeKind()) {
        case RefType::Func:
          globalVal = Val(RefType::func(), AnyRef::null());
          break;
        case RefType::Any:
        case RefType::Null:
          globalVal = Val(RefType::any(), AnyRef::null());
          break;
        case RefType::TypeIndex:
          MOZ_CRASH("Ref NYI");
      }
      break;
  }

  // Override with non-undefined value, if provided.
  RootedValue valueVal(cx, args.get(1));
  if (!valueVal.isUndefined() ||
      (args.length() >= 2 && globalType.isReference())) {
    if (!ToWebAssemblyValue(cx, globalType, valueVal, &globalVal)) {
      return false;
    }
  }

  WasmGlobalObject* global = WasmGlobalObject::create(cx, globalVal, isMutable);
  if (!global) {
    return false;
  }

  args.rval().setObject(*global);
  return true;
}

static bool IsGlobal(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmGlobalObject>();
}

/* static */
bool WasmGlobalObject::valueGetterImpl(JSContext* cx, const CallArgs& args) {
  switch (args.thisv().toObject().as<WasmGlobalObject>().type().kind()) {
    case ValType::I32:
    case ValType::F32:
    case ValType::F64:
      args.thisv().toObject().as<WasmGlobalObject>().value(cx, args.rval());
      return true;
    case ValType::I64:
#ifdef ENABLE_WASM_BIGINT
      if (I64BigIntConversionAvailable(cx)) {
        return args.thisv().toObject().as<WasmGlobalObject>().value(
            cx, args.rval());
      }
#endif
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_I64_TYPE);
      return false;
    case ValType::Ref:
      switch (
          args.thisv().toObject().as<WasmGlobalObject>().type().refTypeKind()) {
        case RefType::Func:
        case RefType::Any:
        case RefType::Null:
          args.thisv().toObject().as<WasmGlobalObject>().value(cx, args.rval());
          return true;
        case RefType::TypeIndex:
          MOZ_CRASH("Ref NYI");
      }
      break;
  }
  MOZ_CRASH();
}

/* static */
bool WasmGlobalObject::valueGetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsGlobal, valueGetterImpl>(cx, args);
}

/* static */
bool WasmGlobalObject::valueSetterImpl(JSContext* cx, const CallArgs& args) {
  if (!args.requireAtLeast(cx, "WebAssembly.Global setter", 1)) {
    return false;
  }

  RootedWasmGlobalObject global(
      cx, &args.thisv().toObject().as<WasmGlobalObject>());
  if (!global->isMutable()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_GLOBAL_IMMUTABLE);
    return false;
  }

  if (global->type() == ValType::I64 && !I64BigIntConversionAvailable(cx)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_I64_TYPE);
    return false;
  }

  RootedVal val(cx);
  if (!ToWebAssemblyValue(cx, global->type(), args.get(0), &val)) {
    return false;
  }
  global->setVal(cx, val);

  args.rval().setUndefined();
  return true;
}

/* static */
bool WasmGlobalObject::valueSetter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsGlobal, valueSetterImpl>(cx, args);
}

const JSPropertySpec WasmGlobalObject::properties[] = {
    JS_PSGS("value", WasmGlobalObject::valueGetter,
            WasmGlobalObject::valueSetter, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Global", JSPROP_READONLY),
    JS_PS_END};

const JSFunctionSpec WasmGlobalObject::methods[] = {
    JS_FN(js_valueOf_str, WasmGlobalObject::valueGetter, 0, JSPROP_ENUMERATE),
    JS_FS_END};

const JSFunctionSpec WasmGlobalObject::static_methods[] = {JS_FS_END};

ValType WasmGlobalObject::type() const {
  return ValType::fromBitsUnsafe(getReservedSlot(TYPE_SLOT).toInt32());
}

bool WasmGlobalObject::isMutable() const {
  return getReservedSlot(MUTABLE_SLOT).toBoolean();
}

void WasmGlobalObject::setVal(JSContext* cx, wasm::HandleVal hval) {
  const Val& val = hval.get();
  Cell* cell = this->cell();
  MOZ_ASSERT(type() == val.type());
  switch (type().kind()) {
    case ValType::I32:
      cell->i32 = val.i32();
      break;
    case ValType::F32:
      cell->f32 = val.f32();
      break;
    case ValType::F64:
      cell->f64 = val.f64();
      break;
    case ValType::I64:
#ifdef ENABLE_WASM_BIGINT
      MOZ_ASSERT(I64BigIntConversionAvailable(cx),
                 "expected BigInt support for setting I64 global");
      cell->i64 = val.i64();
#endif
      break;
    case ValType::Ref:
      switch (this->type().refTypeKind()) {
        case RefType::Func:
        case RefType::Any: {
          AnyRef prevPtr = cell->ref;
          // TODO/AnyRef-boxing: With boxed immediates and strings, the write
          // barrier is going to have to be more complicated.
          ASSERT_ANYREF_IS_JSOBJECT;
          JSObject::writeBarrierPre(prevPtr.asJSObject());
          cell->ref = val.ref();
          if (!cell->ref.isNull()) {
            JSObject::writeBarrierPost(cell->ref.asJSObjectAddress(),
                                       prevPtr.asJSObject(),
                                       cell->ref.asJSObject());
          }
          break;
        }
        case RefType::Null: {
          break;
        }
        case RefType::TypeIndex: {
          MOZ_CRASH("Ref NYI");
        }
      }
      break;
  }
}

void WasmGlobalObject::val(MutableHandleVal outval) const {
  Cell* cell = this->cell();
  switch (type().kind()) {
    case ValType::I32:
      outval.set(Val(uint32_t(cell->i32)));
      return;
    case ValType::I64:
      outval.set(Val(uint64_t(cell->i64)));
      return;
    case ValType::F32:
      outval.set(Val(cell->f32));
      return;
    case ValType::F64:
      outval.set(Val(cell->f64));
      return;
    case ValType::Ref:
      switch (type().refTypeKind()) {
        case RefType::Func:
          outval.set(Val(RefType::func(), cell->ref));
          return;
        case RefType::Any:
        case RefType::Null:
          outval.set(Val(RefType::any(), cell->ref));
          return;
        case RefType::TypeIndex:
          MOZ_CRASH("Ref NYI");
      }
      break;
  }
  MOZ_CRASH("unexpected Global type");
}

bool WasmGlobalObject::value(JSContext* cx, MutableHandleValue out) {
  RootedVal result(cx);
  val(&result);
  return ToJSValue(cx, result.get(), out);
}

WasmGlobalObject::Cell* WasmGlobalObject::cell() const {
  return reinterpret_cast<Cell*>(getReservedSlot(CELL_SLOT).toPrivate());
}

// ============================================================================
// WebAssembly class and static methods

static bool WebAssembly_toSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(cx->names().WebAssembly);
  return true;
}

static bool RejectWithPendingException(JSContext* cx,
                                       Handle<PromiseObject*> promise) {
  if (!cx->isExceptionPending()) {
    return false;
  }

  RootedValue rejectionValue(cx);
  if (!GetAndClearException(cx, &rejectionValue)) {
    return false;
  }

  return PromiseObject::reject(cx, promise, rejectionValue);
}

static bool Reject(JSContext* cx, const CompileArgs& args,
                   Handle<PromiseObject*> promise, const UniqueChars& error) {
  if (!error) {
    ReportOutOfMemory(cx);
    return RejectWithPendingException(cx, promise);
  }

  RootedObject stack(cx, promise->allocationSite());
  RootedString filename(
      cx, JS_NewStringCopyZ(cx, args.scriptedCaller.filename.get()));
  if (!filename) {
    return false;
  }

  unsigned line = args.scriptedCaller.line;

  // Ideally we'd report a JSMSG_WASM_COMPILE_ERROR here, but there's no easy
  // way to create an ErrorObject for an arbitrary error code with multiple
  // replacements.
  UniqueChars str(JS_smprintf("wasm validation error: %s", error.get()));
  if (!str) {
    return false;
  }

  size_t len = strlen(str.get());
  RootedString message(cx, NewStringCopyN<CanGC>(cx, str.get(), len));
  if (!message) {
    return false;
  }

  RootedObject errorObj(
      cx, ErrorObject::create(cx, JSEXN_WASMCOMPILEERROR, stack, filename, 0,
                              line, 0, nullptr, message));
  if (!errorObj) {
    return false;
  }

  RootedValue rejectionValue(cx, ObjectValue(*errorObj));
  return PromiseObject::reject(cx, promise, rejectionValue);
}

static void LogAsync(JSContext* cx, const char* funcName,
                     const Module& module) {
  Log(cx, "async %s succeeded%s", funcName,
      module.loggingDeserialized() ? " (loaded from cache)" : "");
}

enum class Ret { Pair, Instance };

class AsyncInstantiateTask : public OffThreadPromiseTask {
  SharedModule module_;
  PersistentRooted<ImportValues> imports_;
  Ret ret_;

 public:
  AsyncInstantiateTask(JSContext* cx, const Module& module, Ret ret,
                       Handle<PromiseObject*> promise)
      : OffThreadPromiseTask(cx, promise),
        module_(&module),
        imports_(cx),
        ret_(ret) {}

  ImportValues& imports() { return imports_.get(); }

  bool resolve(JSContext* cx, Handle<PromiseObject*> promise) override {
    RootedObject instanceProto(
        cx, &cx->global()->getPrototype(JSProto_WasmInstance).toObject());

    RootedWasmInstanceObject instanceObj(cx);
    if (!module_->instantiate(cx, imports_.get(), instanceProto,
                              &instanceObj)) {
      return RejectWithPendingException(cx, promise);
    }

    RootedValue resolutionValue(cx);
    if (ret_ == Ret::Instance) {
      resolutionValue = ObjectValue(*instanceObj);
    } else {
      RootedObject resultObj(cx, JS_NewPlainObject(cx));
      if (!resultObj) {
        return RejectWithPendingException(cx, promise);
      }

      RootedObject moduleProto(
          cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
      RootedObject moduleObj(
          cx, WasmModuleObject::create(cx, *module_, moduleProto));
      if (!moduleObj) {
        return RejectWithPendingException(cx, promise);
      }

      RootedValue val(cx, ObjectValue(*moduleObj));
      if (!JS_DefineProperty(cx, resultObj, "module", val, JSPROP_ENUMERATE)) {
        return RejectWithPendingException(cx, promise);
      }

      val = ObjectValue(*instanceObj);
      if (!JS_DefineProperty(cx, resultObj, "instance", val,
                             JSPROP_ENUMERATE)) {
        return RejectWithPendingException(cx, promise);
      }

      resolutionValue = ObjectValue(*resultObj);
    }

    if (!PromiseObject::resolve(cx, promise, resolutionValue)) {
      return RejectWithPendingException(cx, promise);
    }

    LogAsync(cx, "instantiate", *module_);
    return true;
  }
};

static bool AsyncInstantiate(JSContext* cx, const Module& module,
                             HandleObject importObj, Ret ret,
                             Handle<PromiseObject*> promise) {
  auto task = js::MakeUnique<AsyncInstantiateTask>(cx, module, ret, promise);
  if (!task || !task->init(cx)) {
    return false;
  }

  if (!GetImports(cx, module, importObj, &task->imports())) {
    return RejectWithPendingException(cx, promise);
  }

  task.release()->dispatchResolveAndDestroy();
  return true;
}

static bool ResolveCompile(JSContext* cx, const Module& module,
                           Handle<PromiseObject*> promise) {
  RootedObject proto(
      cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
  RootedObject moduleObj(cx, WasmModuleObject::create(cx, module, proto));
  if (!moduleObj) {
    return RejectWithPendingException(cx, promise);
  }

  RootedValue resolutionValue(cx, ObjectValue(*moduleObj));
  if (!PromiseObject::resolve(cx, promise, resolutionValue)) {
    return RejectWithPendingException(cx, promise);
  }

  LogAsync(cx, "compile", module);
  return true;
}

struct CompileBufferTask : PromiseHelperTask {
  MutableBytes bytecode;
  SharedCompileArgs compileArgs;
  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module;
  bool instantiate;
  PersistentRootedObject importObj;

  CompileBufferTask(JSContext* cx, Handle<PromiseObject*> promise,
                    HandleObject importObj)
      : PromiseHelperTask(cx, promise),
        instantiate(true),
        importObj(cx, importObj) {}

  CompileBufferTask(JSContext* cx, Handle<PromiseObject*> promise)
      : PromiseHelperTask(cx, promise), instantiate(false) {}

  bool init(JSContext* cx, const char* introducer) {
    compileArgs = InitCompileArgs(cx, introducer);
    if (!compileArgs) {
      return false;
    }
    return PromiseHelperTask::init(cx);
  }

  void execute() override {
    module = CompileBuffer(*compileArgs, *bytecode, &error, &warnings);
  }

  bool resolve(JSContext* cx, Handle<PromiseObject*> promise) override {
    if (!module) {
      return Reject(cx, *compileArgs, promise, error);
    }
    if (!ReportCompileWarnings(cx, warnings)) {
      return false;
    }
    if (instantiate) {
      return AsyncInstantiate(cx, *module, importObj, Ret::Pair, promise);
    }
    return ResolveCompile(cx, *module, promise);
  }
};

static bool RejectWithPendingException(JSContext* cx,
                                       Handle<PromiseObject*> promise,
                                       CallArgs& callArgs) {
  if (!RejectWithPendingException(cx, promise)) {
    return false;
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool EnsurePromiseSupport(JSContext* cx) {
  if (!cx->runtime()->offThreadPromiseState.ref().initialized()) {
    JS_ReportErrorASCII(
        cx, "WebAssembly Promise APIs not supported in this runtime.");
    return false;
  }
  return true;
}

static bool GetBufferSource(JSContext* cx, CallArgs callArgs, const char* name,
                            MutableBytes* bytecode) {
  if (!callArgs.requireAtLeast(cx, name, 1)) {
    return false;
  }

  if (!callArgs[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_ARG);
    return false;
  }

  return GetBufferSource(cx, &callArgs[0].toObject(), JSMSG_WASM_BAD_BUF_ARG,
                         bytecode);
}

static bool WebAssembly_compile(JSContext* cx, unsigned argc, Value* vp) {
  if (!EnsurePromiseSupport(cx)) {
    return false;
  }

  Log(cx, "async compile() started");

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  auto task = cx->make_unique<CompileBufferTask>(cx, promise);
  if (!task || !task->init(cx, "WebAssembly.compile")) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  if (!GetBufferSource(cx, callArgs, "WebAssembly.compile", &task->bytecode)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  if (!StartOffThreadPromiseHelperTask(cx, std::move(task))) {
    return false;
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool GetInstantiateArgs(JSContext* cx, CallArgs callArgs,
                               MutableHandleObject firstArg,
                               MutableHandleObject importObj) {
  if (!callArgs.requireAtLeast(cx, "WebAssembly.instantiate", 1)) {
    return false;
  }

  if (!callArgs[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_MOD_ARG);
    return false;
  }

  firstArg.set(&callArgs[0].toObject());

  return GetImportArg(cx, callArgs, importObj);
}

static bool WebAssembly_instantiate(JSContext* cx, unsigned argc, Value* vp) {
  if (!EnsurePromiseSupport(cx)) {
    return false;
  }

  Log(cx, "async instantiate() started");

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  RootedObject firstArg(cx);
  RootedObject importObj(cx);
  if (!GetInstantiateArgs(cx, callArgs, &firstArg, &importObj)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  const Module* module;
  if (IsModuleObject(firstArg, &module)) {
    if (!AsyncInstantiate(cx, *module, importObj, Ret::Instance, promise)) {
      return false;
    }
  } else {
    auto task = cx->make_unique<CompileBufferTask>(cx, promise, importObj);
    if (!task || !task->init(cx, "WebAssembly.instantiate")) {
      return false;
    }

    if (!GetBufferSource(cx, firstArg, JSMSG_WASM_BAD_BUF_MOD_ARG,
                         &task->bytecode)) {
      return RejectWithPendingException(cx, promise, callArgs);
    }

    if (!StartOffThreadPromiseHelperTask(cx, std::move(task))) {
      return false;
    }
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool WebAssembly_validate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs callArgs = CallArgsFromVp(argc, vp);

  MutableBytes bytecode;
  if (!GetBufferSource(cx, callArgs, "WebAssembly.validate", &bytecode)) {
    return false;
  }

  UniqueChars error;
  bool validated = Validate(cx, *bytecode, &error);

  // If the reason for validation failure was OOM (signalled by null error
  // message), report out-of-memory so that validate's return is always
  // correct.
  if (!validated && !error) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (error) {
    MOZ_ASSERT(!validated);
    Log(cx, "validate() failed with: %s", error.get());
  }

  callArgs.rval().setBoolean(validated);
  return true;
}

static bool EnsureStreamSupport(JSContext* cx) {
  // This should match wasm::StreamingCompilationAvailable().

  if (!EnsurePromiseSupport(cx)) {
    return false;
  }

  if (!CanUseExtraThreads()) {
    JS_ReportErrorASCII(
        cx, "WebAssembly.compileStreaming not supported with --no-threads");
    return false;
  }

  if (!cx->runtime()->consumeStreamCallback) {
    JS_ReportErrorASCII(cx,
                        "WebAssembly streaming not supported in this runtime");
    return false;
  }

  return true;
}

// This value is chosen and asserted to be disjoint from any host error code.
static const size_t StreamOOMCode = 0;

static bool RejectWithStreamErrorNumber(JSContext* cx, size_t errorCode,
                                        Handle<PromiseObject*> promise) {
  if (errorCode == StreamOOMCode) {
    ReportOutOfMemory(cx);
    return false;
  }

  cx->runtime()->reportStreamErrorCallback(cx, errorCode);
  return RejectWithPendingException(cx, promise);
}

class CompileStreamTask : public PromiseHelperTask, public JS::StreamConsumer {
  // The stream progresses monotonically through these states; the helper
  // thread wait()s for streamState_ to reach Closed.
  enum StreamState { Env, Code, Tail, Closed };
  ExclusiveWaitableData<StreamState> streamState_;

  // Immutable:
  const bool instantiate_;
  const PersistentRootedObject importObj_;

  // Immutable after noteResponseURLs() which is called at most once before
  // first call on stream thread:
  const MutableCompileArgs compileArgs_;

  // Immutable after Env state:
  Bytes envBytes_;
  SectionRange codeSection_;

  // The code section vector is resized once during the Env state and filled
  // in chunk by chunk during the Code state, updating the end-pointer after
  // each chunk:
  Bytes codeBytes_;
  uint8_t* codeBytesEnd_;
  ExclusiveBytesPtr exclusiveCodeBytesEnd_;

  // Immutable after Tail state:
  Bytes tailBytes_;
  ExclusiveStreamEndData exclusiveStreamEnd_;

  // Written once before Closed state and read in Closed state on main thread:
  SharedModule module_;
  Maybe<size_t> streamError_;
  UniqueChars compileError_;
  UniqueCharsVector warnings_;

  // Set on stream thread and read racily on helper thread to abort compilation:
  Atomic<bool> streamFailed_;

  // Called on some thread before consumeChunk(), streamEnd(), streamError()):

  void noteResponseURLs(const char* url, const char* sourceMapUrl) override {
    if (url) {
      compileArgs_->scriptedCaller.filename = DuplicateString(url);
      compileArgs_->scriptedCaller.filenameIsURL = true;
    }
    if (sourceMapUrl) {
      compileArgs_->sourceMapURL = DuplicateString(sourceMapUrl);
    }
  }

  // Called on a stream thread:

  // Until StartOffThreadPromiseHelperTask succeeds, we are responsible for
  // dispatching ourselves back to the JS thread.
  //
  // Warning: After this function returns, 'this' can be deleted at any time, so
  // the caller must immediately return from the stream callback.
  void setClosedAndDestroyBeforeHelperThreadStarted() {
    streamState_.lock().get() = Closed;
    dispatchResolveAndDestroy();
  }

  // See setClosedAndDestroyBeforeHelperThreadStarted() comment.
  bool rejectAndDestroyBeforeHelperThreadStarted(size_t errorNumber) {
    MOZ_ASSERT(streamState_.lock() == Env);
    MOZ_ASSERT(!streamError_);
    streamError_ = Some(errorNumber);
    setClosedAndDestroyBeforeHelperThreadStarted();
    return false;
  }

  // Once StartOffThreadPromiseHelperTask succeeds, the helper thread will
  // dispatchResolveAndDestroy() after execute() returns, but execute()
  // wait()s for state to be Closed.
  //
  // Warning: After this function returns, 'this' can be deleted at any time, so
  // the caller must immediately return from the stream callback.
  void setClosedAndDestroyAfterHelperThreadStarted() {
    auto streamState = streamState_.lock();
    MOZ_ASSERT(streamState != Closed);
    streamState.get() = Closed;
    streamState.notify_one(/* stream closed */);
  }

  // See setClosedAndDestroyAfterHelperThreadStarted() comment.
  bool rejectAndDestroyAfterHelperThreadStarted(size_t errorNumber) {
    MOZ_ASSERT(!streamError_);
    streamError_ = Some(errorNumber);
    streamFailed_ = true;
    exclusiveCodeBytesEnd_.lock().notify_one();
    exclusiveStreamEnd_.lock().notify_one();
    setClosedAndDestroyAfterHelperThreadStarted();
    return false;
  }

  bool consumeChunk(const uint8_t* begin, size_t length) override {
    switch (streamState_.lock().get()) {
      case Env: {
        if (!envBytes_.append(begin, length)) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        if (!StartsCodeSection(envBytes_.begin(), envBytes_.end(),
                               &codeSection_)) {
          return true;
        }

        uint32_t extraBytes = envBytes_.length() - codeSection_.start;
        if (extraBytes) {
          envBytes_.shrinkTo(codeSection_.start);
        }

        if (codeSection_.size > MaxCodeSectionBytes) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        if (!codeBytes_.resize(codeSection_.size)) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        codeBytesEnd_ = codeBytes_.begin();
        exclusiveCodeBytesEnd_.lock().get() = codeBytesEnd_;

        if (!StartOffThreadPromiseHelperTask(this)) {
          return rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
        }

        // Set the state to Code iff StartOffThreadPromiseHelperTask()
        // succeeds so that the state tells us whether we are before or
        // after the helper thread started.
        streamState_.lock().get() = Code;

        if (extraBytes) {
          return consumeChunk(begin + length - extraBytes, extraBytes);
        }

        return true;
      }
      case Code: {
        size_t copyLength =
            std::min<size_t>(length, codeBytes_.end() - codeBytesEnd_);
        memcpy(codeBytesEnd_, begin, copyLength);
        codeBytesEnd_ += copyLength;

        {
          auto codeStreamEnd = exclusiveCodeBytesEnd_.lock();
          codeStreamEnd.get() = codeBytesEnd_;
          codeStreamEnd.notify_one();
        }

        if (codeBytesEnd_ != codeBytes_.end()) {
          return true;
        }

        streamState_.lock().get() = Tail;

        if (uint32_t extraBytes = length - copyLength) {
          return consumeChunk(begin + copyLength, extraBytes);
        }

        return true;
      }
      case Tail: {
        if (!tailBytes_.append(begin, length)) {
          return rejectAndDestroyAfterHelperThreadStarted(StreamOOMCode);
        }

        return true;
      }
      case Closed:
        MOZ_CRASH("consumeChunk() in Closed state");
    }
    MOZ_CRASH("unreachable");
  }

  void streamEnd(JS::OptimizedEncodingListener* tier2Listener) override {
    switch (streamState_.lock().get()) {
      case Env: {
        SharedBytes bytecode = js_new<ShareableBytes>(std::move(envBytes_));
        if (!bytecode) {
          rejectAndDestroyBeforeHelperThreadStarted(StreamOOMCode);
          return;
        }
        module_ =
            CompileBuffer(*compileArgs_, *bytecode, &compileError_, &warnings_);
        setClosedAndDestroyBeforeHelperThreadStarted();
        return;
      }
      case Code:
      case Tail:
        // Unlock exclusiveStreamEnd_ before locking streamState_.
        {
          auto streamEnd = exclusiveStreamEnd_.lock();
          MOZ_ASSERT(!streamEnd->reached);
          streamEnd->reached = true;
          streamEnd->tailBytes = &tailBytes_;
          streamEnd->tier2Listener = tier2Listener;
          streamEnd.notify_one();
        }
        setClosedAndDestroyAfterHelperThreadStarted();
        return;
      case Closed:
        MOZ_CRASH("streamEnd() in Closed state");
    }
  }

  void streamError(size_t errorCode) override {
    MOZ_ASSERT(errorCode != StreamOOMCode);
    switch (streamState_.lock().get()) {
      case Env:
        rejectAndDestroyBeforeHelperThreadStarted(errorCode);
        return;
      case Tail:
      case Code:
        rejectAndDestroyAfterHelperThreadStarted(errorCode);
        return;
      case Closed:
        MOZ_CRASH("streamError() in Closed state");
    }
  }

  void consumeOptimizedEncoding(const uint8_t* begin, size_t length) override {
    module_ = Module::deserialize(begin, length);

    MOZ_ASSERT(streamState_.lock().get() == Env);
    setClosedAndDestroyBeforeHelperThreadStarted();
  }

  // Called on a helper thread:

  void execute() override {
    module_ = CompileStreaming(*compileArgs_, envBytes_, codeBytes_,
                               exclusiveCodeBytesEnd_, exclusiveStreamEnd_,
                               streamFailed_, &compileError_, &warnings_);

    // When execute() returns, the CompileStreamTask will be dispatched
    // back to its JS thread to call resolve() and then be destroyed. We
    // can't let this happen until the stream has been closed lest
    // consumeChunk() or streamEnd() be called on a dead object.
    auto streamState = streamState_.lock();
    while (streamState != Closed) {
      streamState.wait(/* stream closed */);
    }
  }

  // Called on a JS thread after streaming compilation completes/errors:

  bool resolve(JSContext* cx, Handle<PromiseObject*> promise) override {
    MOZ_ASSERT(streamState_.lock() == Closed);

    if (module_) {
      MOZ_ASSERT(!streamFailed_ && !streamError_ && !compileError_);
      if (!ReportCompileWarnings(cx, warnings_)) {
        return false;
      }
      if (instantiate_) {
        return AsyncInstantiate(cx, *module_, importObj_, Ret::Pair, promise);
      }
      return ResolveCompile(cx, *module_, promise);
    }

    if (streamError_) {
      return RejectWithStreamErrorNumber(cx, *streamError_, promise);
    }

    return Reject(cx, *compileArgs_, promise, compileError_);
  }

 public:
  CompileStreamTask(JSContext* cx, Handle<PromiseObject*> promise,
                    CompileArgs& compileArgs, bool instantiate,
                    HandleObject importObj)
      : PromiseHelperTask(cx, promise),
        streamState_(mutexid::WasmStreamStatus, Env),
        instantiate_(instantiate),
        importObj_(cx, importObj),
        compileArgs_(&compileArgs),
        codeSection_{},
        codeBytesEnd_(nullptr),
        exclusiveCodeBytesEnd_(mutexid::WasmCodeBytesEnd, nullptr),
        exclusiveStreamEnd_(mutexid::WasmStreamEnd),
        streamFailed_(false) {
    MOZ_ASSERT_IF(importObj_, instantiate_);
  }
};

// A short-lived object that captures the arguments of a
// WebAssembly.{compileStreaming,instantiateStreaming} while waiting for
// the Promise<Response> to resolve to a (hopefully) Promise.
class ResolveResponseClosure : public NativeObject {
  static const unsigned COMPILE_ARGS_SLOT = 0;
  static const unsigned PROMISE_OBJ_SLOT = 1;
  static const unsigned INSTANTIATE_SLOT = 2;
  static const unsigned IMPORT_OBJ_SLOT = 3;
  static const JSClassOps classOps_;

  static void finalize(JSFreeOp* fop, JSObject* obj) {
    auto& closure = obj->as<ResolveResponseClosure>();
    fop->release(obj, &closure.compileArgs(),
                 MemoryUse::WasmResolveResponseClosure);
  }

 public:
  static const unsigned RESERVED_SLOTS = 4;
  static const JSClass class_;

  static ResolveResponseClosure* create(JSContext* cx, const CompileArgs& args,
                                        HandleObject promise, bool instantiate,
                                        HandleObject importObj) {
    MOZ_ASSERT_IF(importObj, instantiate);

    AutoSetNewObjectMetadata metadata(cx);
    auto* obj = NewObjectWithGivenProto<ResolveResponseClosure>(cx, nullptr);
    if (!obj) {
      return nullptr;
    }

    args.AddRef();
    InitReservedSlot(obj, COMPILE_ARGS_SLOT, const_cast<CompileArgs*>(&args),
                     MemoryUse::WasmResolveResponseClosure);
    obj->setReservedSlot(PROMISE_OBJ_SLOT, ObjectValue(*promise));
    obj->setReservedSlot(INSTANTIATE_SLOT, BooleanValue(instantiate));
    obj->setReservedSlot(IMPORT_OBJ_SLOT, ObjectOrNullValue(importObj));
    return obj;
  }

  CompileArgs& compileArgs() const {
    return *(CompileArgs*)getReservedSlot(COMPILE_ARGS_SLOT).toPrivate();
  }
  PromiseObject& promise() const {
    return getReservedSlot(PROMISE_OBJ_SLOT).toObject().as<PromiseObject>();
  }
  bool instantiate() const {
    return getReservedSlot(INSTANTIATE_SLOT).toBoolean();
  }
  JSObject* importObj() const {
    return getReservedSlot(IMPORT_OBJ_SLOT).toObjectOrNull();
  }
};

const JSClassOps ResolveResponseClosure::classOps_ = {
    nullptr,                           // addProperty
    nullptr,                           // delProperty
    nullptr,                           // enumerate
    nullptr,                           // newEnumerate
    nullptr,                           // resolve
    nullptr,                           // mayResolve
    ResolveResponseClosure::finalize,  // finalize
    nullptr,                           // call
    nullptr,                           // hasInstance
    nullptr,                           // construct
    nullptr,                           // trace
};

const JSClass ResolveResponseClosure::class_ = {
    "WebAssembly ResolveResponseClosure",
    JSCLASS_DELAY_METADATA_BUILDER |
        JSCLASS_HAS_RESERVED_SLOTS(ResolveResponseClosure::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &ResolveResponseClosure::classOps_,
};

static ResolveResponseClosure* ToResolveResponseClosure(CallArgs args) {
  return &args.callee()
              .as<JSFunction>()
              .getExtendedSlot(0)
              .toObject()
              .as<ResolveResponseClosure>();
}

static bool RejectWithErrorNumber(JSContext* cx, uint32_t errorNumber,
                                  Handle<PromiseObject*> promise) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, errorNumber);
  return RejectWithPendingException(cx, promise);
}

static bool ResolveResponse_OnFulfilled(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs callArgs = CallArgsFromVp(argc, vp);

  Rooted<ResolveResponseClosure*> closure(cx,
                                          ToResolveResponseClosure(callArgs));
  Rooted<PromiseObject*> promise(cx, &closure->promise());
  CompileArgs& compileArgs = closure->compileArgs();
  bool instantiate = closure->instantiate();
  Rooted<JSObject*> importObj(cx, closure->importObj());

  auto task = cx->make_unique<CompileStreamTask>(cx, promise, compileArgs,
                                                 instantiate, importObj);
  if (!task || !task->init(cx)) {
    return false;
  }

  if (!callArgs.get(0).isObject()) {
    return RejectWithErrorNumber(cx, JSMSG_BAD_RESPONSE_VALUE, promise);
  }

  RootedObject response(cx, &callArgs.get(0).toObject());
  if (!cx->runtime()->consumeStreamCallback(cx, response, JS::MimeType::Wasm,
                                            task.get())) {
    return RejectWithPendingException(cx, promise);
  }

  Unused << task.release();

  callArgs.rval().setUndefined();
  return true;
}

static bool ResolveResponse_OnRejected(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<ResolveResponseClosure*> closure(cx, ToResolveResponseClosure(args));
  Rooted<PromiseObject*> promise(cx, &closure->promise());

  if (!PromiseObject::reject(cx, promise, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool ResolveResponse(JSContext* cx, CallArgs callArgs,
                            Handle<PromiseObject*> promise,
                            bool instantiate = false,
                            HandleObject importObj = nullptr) {
  MOZ_ASSERT_IF(importObj, instantiate);

  const char* introducer = instantiate ? "WebAssembly.instantiateStreaming"
                                       : "WebAssembly.compileStreaming";

  SharedCompileArgs compileArgs = InitCompileArgs(cx, introducer);
  if (!compileArgs) {
    return false;
  }

  RootedObject closure(
      cx, ResolveResponseClosure::create(cx, *compileArgs, promise, instantiate,
                                         importObj));
  if (!closure) {
    return false;
  }

  RootedFunction onResolved(
      cx, NewNativeFunction(cx, ResolveResponse_OnFulfilled, 1, nullptr,
                            gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
  if (!onResolved) {
    return false;
  }

  RootedFunction onRejected(
      cx, NewNativeFunction(cx, ResolveResponse_OnRejected, 1, nullptr,
                            gc::AllocKind::FUNCTION_EXTENDED, GenericObject));
  if (!onRejected) {
    return false;
  }

  onResolved->setExtendedSlot(0, ObjectValue(*closure));
  onRejected->setExtendedSlot(0, ObjectValue(*closure));

  RootedObject resolve(cx,
                       PromiseObject::unforgeableResolve(cx, callArgs.get(0)));
  if (!resolve) {
    return false;
  }

  return JS::AddPromiseReactions(cx, resolve, onResolved, onRejected);
}

static bool WebAssembly_compileStreaming(JSContext* cx, unsigned argc,
                                         Value* vp) {
  if (!EnsureStreamSupport(cx)) {
    return false;
  }

  Log(cx, "async compileStreaming() started");

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  if (!ResolveResponse(cx, callArgs, promise)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static bool WebAssembly_instantiateStreaming(JSContext* cx, unsigned argc,
                                             Value* vp) {
  if (!EnsureStreamSupport(cx)) {
    return false;
  }

  Log(cx, "async instantiateStreaming() started");

  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return false;
  }

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  RootedObject firstArg(cx);
  RootedObject importObj(cx);
  if (!GetInstantiateArgs(cx, callArgs, &firstArg, &importObj)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  if (!ResolveResponse(cx, callArgs, promise, true, importObj)) {
    return RejectWithPendingException(cx, promise, callArgs);
  }

  callArgs.rval().setObject(*promise);
  return true;
}

static const JSFunctionSpec WebAssembly_static_methods[] = {
    JS_FN(js_toSource_str, WebAssembly_toSource, 0, 0),
    JS_FN("compile", WebAssembly_compile, 1, JSPROP_ENUMERATE),
    JS_FN("instantiate", WebAssembly_instantiate, 1, JSPROP_ENUMERATE),
    JS_FN("validate", WebAssembly_validate, 1, JSPROP_ENUMERATE),
    JS_FN("compileStreaming", WebAssembly_compileStreaming, 1,
          JSPROP_ENUMERATE),
    JS_FN("instantiateStreaming", WebAssembly_instantiateStreaming, 1,
          JSPROP_ENUMERATE),
    JS_FS_END};

static JSObject* CreateWebAssemblyObject(JSContext* cx, JSProtoKey key) {
  MOZ_RELEASE_ASSERT(HasSupport(cx));

  Handle<GlobalObject*> global = cx->global();
  RootedObject proto(cx, GlobalObject::getOrCreateObjectPrototype(cx, global));
  if (!proto) {
    return nullptr;
  }
  return NewObjectWithGivenProto(cx, &WebAssemblyClass, proto, SingletonObject);
}

static bool WebAssemblyClassFinish(JSContext* cx, HandleObject wasm,
                                   HandleObject proto) {
  struct NameAndProtoKey {
    const char* const name;
    JSProtoKey key;
  };

  constexpr NameAndProtoKey entries[] = {
      {"Module", JSProto_WasmModule},
      {"Instance", JSProto_WasmInstance},
      {"Memory", JSProto_WasmMemory},
      {"Table", JSProto_WasmTable},
      {"Global", JSProto_WasmGlobal},
      {"CompileError", GetExceptionProtoKey(JSEXN_WASMCOMPILEERROR)},
      {"LinkError", GetExceptionProtoKey(JSEXN_WASMLINKERROR)},
      {"RuntimeError", GetExceptionProtoKey(JSEXN_WASMRUNTIMEERROR)},
  };

  RootedValue ctorValue(cx);
  RootedId id(cx);
  for (const auto& entry : entries) {
    const char* name = entry.name;
    JSProtoKey key = entry.key;

    JSObject* ctor = GlobalObject::getOrCreateConstructor(cx, key);
    if (!ctor) {
      return false;
    }
    ctorValue.setObject(*ctor);

    JSAtom* className = Atomize(cx, name, strlen(name));
    if (!className) {
      return false;
    }
    id.set(AtomToId(className));

    if (!DefineDataProperty(cx, wasm, id, ctorValue, 0)) {
      return false;
    }
  }

  return true;
}

static const ClassSpec WebAssemblyClassSpec = {CreateWebAssemblyObject,
                                               nullptr,
                                               WebAssembly_static_methods,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               WebAssemblyClassFinish};

const JSClass js::WebAssemblyClass = {
    js_WebAssembly_str, JSCLASS_HAS_CACHED_PROTO(JSProto_WebAssembly),
    JS_NULL_CLASS_OPS, &WebAssemblyClassSpec};
