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

#include "jsapi.h"

#include "ds/IdValuePair.h"  // js::IdValuePair
#include "gc/FreeOp.h"
#include "jit/AtomicOperations.h"
#include "jit/JitContext.h"
#include "jit/JitOptions.h"
#include "jit/Simulator.h"
#include "js/ForOfIterator.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/Printf.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_GetProperty
#include "js/PropertySpec.h"        // JS_{PS,FN}{,_END}
#include "js/StreamConsumer.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/ErrorObject.h"
#include "vm/FunctionFlags.h"      // js::FunctionFlags
#include "vm/GlobalObject.h"       // js::GlobalObject
#include "vm/HelperThreadState.h"  // js::PromiseHelperTask
#include "vm/Interpreter.h"
#include "vm/JSFunction.h"
#include "vm/PlainObject.h"    // js::PlainObject
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/StringType.h"
#include "vm/Warnings.h"       // js::WarnNumberASCII
#include "vm/WellKnownAtom.h"  // js_*_str
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmCraneliftCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmIntrinsic.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmProcess.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmValidate.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

/*
 * [SMDOC] WebAssembly code rules (evolving)
 *
 * TlsContext.get() is only to be invoked from functions that have been invoked
 *   _directly_ by generated code as cold(!) Builtin calls, from code that is
 *   only used by signal handlers, or from helper functions that have been
 *   called _directly_ from a simulator.  All other code shall pass in a
 *   JSContext* to functions that need it, or an Instance* or TlsData* since the
 *   context is available through them.
 *
 *   Code that uses TlsContext.get() shall annotate each such call with the
 *   reason why the call is OK.
 */

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::Nothing;
using mozilla::RangedPtr;
using mozilla::Span;

// About the fuzzer intercession points: If fuzzing has been selected and only a
// single compiler has been selected then we will disable features that are not
// supported by that single compiler.  This is strictly a concession to the
// fuzzer infrastructure.

static inline bool IsFuzzingIon(JSContext* cx) {
  return IsFuzzing() && !cx->options().wasmBaseline() &&
         cx->options().wasmIon() && !cx->options().wasmCranelift();
}

static inline bool IsFuzzingCranelift(JSContext* cx) {
  return IsFuzzing() && !cx->options().wasmBaseline() &&
         !cx->options().wasmIon() && cx->options().wasmCranelift();
}

// These functions read flags and apply fuzzing intercession policies.  Never go
// directly to the flags in code below, always go via these accessors.

#ifdef ENABLE_WASM_SIMD_WORMHOLE
static inline bool WasmSimdWormholeFlag(JSContext* cx) {
  return cx->options().wasmSimdWormhole();
}
#endif

static inline bool WasmThreadsFlag(JSContext* cx) {
  return cx->realm() &&
         cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled();
}

#define WASM_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, FLAG_PRED, \
                     ...)                                                      \
  static inline bool Wasm##NAME##Flag(JSContext* cx) {                         \
    return (COMPILE_PRED) && (FLAG_PRED) && cx->options().wasm##NAME();        \
  }
JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE, WASM_FEATURE);
#undef WASM_FEATURE

static inline bool WasmDebuggerActive(JSContext* cx) {
  if (IsFuzzingIon(cx) || IsFuzzingCranelift(cx)) {
    return false;
  }
  return cx->realm() && cx->realm()->debuggerObservesAsmJS();
}

/*
 * [SMDOC] Compiler and feature selection; compiler and feature availability.
 *
 * In order to make the computation of whether a wasm feature or wasm compiler
 * is available predictable, we have established some rules, and implemented
 * those rules.
 *
 * Code elsewhere should use the predicates below to test for features and
 * compilers, it should never try to compute feature and compiler availability
 * in other ways.
 *
 * At the outset, there is a set of selected compilers C containing at most one
 * baseline compiler [*] and at most one optimizing compiler [**], and a set of
 * selected features F.  These selections come from defaults and from overrides
 * by command line switches in the shell and javascript.option.wasm_X in the
 * browser.  Defaults for both features and compilers may be platform specific,
 * for example, some compilers may not be available on some platforms because
 * they do not support the architecture at all or they do not support features
 * that must be enabled by default on the platform.
 *
 * [*] Currently we have only one, "baseline" aka "Rabaldr", but other
 *     implementations have additional baseline translators, eg from wasm
 *     bytecode to an internal code processed by an interpreter.
 *
 * [**] Currently we have two, "ion" aka "Baldr", and "Cranelift".
 *
 *
 * Compiler availability:
 *
 * The set of features F induces a set of available compilers A: these are the
 * compilers that all support all the features in F.  (Some of these compilers
 * may not be in the set C.)
 *
 * The sets C and A are intersected, yielding a set of enabled compilers E.
 * Notably, the set E may be empty, in which case wasm is effectively disabled
 * (though the WebAssembly object is still present in the global environment).
 *
 * An important consequence is that selecting a feature that is not supported by
 * a particular compiler disables that compiler completely -- there is no notion
 * of a compiler being available but suddenly failing when an unsupported
 * feature is used by a program.  If a compiler is available, it supports all
 * the features that have been selected.
 *
 * Equally important, a feature cannot be enabled by default on a platform if
 * the feature is not supported by all the compilers we wish to have enabled by
 * default on the platform.  We MUST by-default disable features on a platform
 * that are not supported by all the compilers on the platform.
 *
 * As an example:
 *
 *   On ARM64 the default compilers are Baseline and Cranelift.  Say Cranelift
 *   does not support feature X.  Thus X cannot be enabled by default on ARM64.
 *   However, X support can be compiled-in to SpiderMonkey, and the user can opt
 *   to enable X.  Doing so will disable Cranelift.
 *
 *   In contrast, X can be enabled by default on x64, where the default
 *   compilers are Baseline and Ion, both of which support X.
 *
 *   A subtlety is worth noting: on x64, enabling Cranelift (thus disabling Ion)
 *   will not disable X.  Instead, the presence of X in the selected feature set
 *   will disable Cranelift, leaving only Baseline.  This follows from the logic
 *   described above.
 *
 * In a shell build, the testing functions wasmCompilersPresent,
 * wasmCompileMode, wasmCraneliftDisabledByFeatures, and
 * wasmIonDisabledByFeatures can be used to probe compiler availability and the
 * reasons for a compiler being unavailable.
 *
 *
 * Feature availability:
 *
 * A feature is available if it is selected and there is at least one available
 * compiler that implements it.
 *
 * For example, --wasm-gc selects the GC feature, and if Baseline is available
 * then the feature is available.
 *
 * In a shell build, there are per-feature testing functions (of the form
 * wasmFeatureEnabled) to probe whether specific features are available.
 */

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

bool wasm::WasmCompilerForAsmJSAvailable(JSContext* cx) {
  // For now, restrict this to Ion - we have not tested Cranelift properly.
  return IonAvailable(cx);
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
  bool debug = WasmDebuggerActive(cx);
  bool functionReferences = WasmFunctionReferencesFlag(cx);
  bool gc = WasmGcFlag(cx);
  bool exn = WasmExceptionsFlag(cx);
  if (reason) {
    char sep = 0;
    if (debug && !Append(reason, "debug", &sep)) {
      return false;
    }
    if (functionReferences && !Append(reason, "function-references", &sep)) {
      return false;
    }
    if (gc && !Append(reason, "gc", &sep)) {
      return false;
    }
    if (exn && !Append(reason, "exceptions", &sep)) {
      return false;
    }
  }
  *isDisabled = debug || functionReferences || gc || exn;
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
  // Cranelift has no debugging support, no gc support, no simd, and
  // no exceptions support.
  bool debug = WasmDebuggerActive(cx);
  bool functionReferences = WasmFunctionReferencesFlag(cx);
  bool gc = WasmGcFlag(cx);
#ifdef JS_CODEGEN_ARM64
  // Cranelift aarch64 has full SIMD support.
  bool simdOnNonAarch64 = false;
#else
  bool simdOnNonAarch64 = WasmSimdFlag(cx);
#endif
  bool exn = WasmExceptionsFlag(cx);
  if (reason) {
    char sep = 0;
    if (debug && !Append(reason, "debug", &sep)) {
      return false;
    }
    if (functionReferences && !Append(reason, "function-references", &sep)) {
      return false;
    }
    if (gc && !Append(reason, "gc", &sep)) {
      return false;
    }
    if (simdOnNonAarch64 && !Append(reason, "simd", &sep)) {
      return false;
    }
    if (exn && !Append(reason, "exceptions", &sep)) {
      return false;
    }
  }
  *isDisabled = debug || functionReferences || gc || simdOnNonAarch64 || exn;
  return true;
}

bool wasm::AnyCompilerAvailable(JSContext* cx) {
  return wasm::BaselineAvailable(cx) || wasm::IonAvailable(cx) ||
         wasm::CraneliftAvailable(cx);
}

// Feature predicates.  These must be kept in sync with the predicates in the
// section above.
//
// The meaning of these predicates is tricky: A predicate is true for a feature
// if the feature is enabled and/or compiled-in *and* we have *at least one*
// compiler that can support the feature.  Subsequent compiler selection must
// ensure that only compilers that actually support the feature are used.

#define WASM_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, FLAG_PRED, \
                     ...)                                                      \
  bool wasm::NAME##Available(JSContext* cx) {                                  \
    return Wasm##NAME##Flag(cx) && (COMPILER_PRED);                            \
  }
JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE, WASM_FEATURE)
#undef WASM_FEATURE

bool wasm::IsSimdPrivilegedContext(JSContext* cx) {
  // This may be slightly more lenient than we want in an ideal world, but it
  // remains safe.
  return cx->realm() && cx->realm()->principals() &&
         cx->realm()->principals()->isSystemOrAddonPrincipal();
}

bool wasm::SimdWormholeAvailable(JSContext* cx) {
#ifdef ENABLE_WASM_SIMD_WORMHOLE
  // The #ifdef ensures that we only enable the wormhole on hardware that
  // supports it and if SIMD support is compiled in.
  //
  // Next we must check that the CPU supports SIMD; it might not, even if SIMD
  // is available.  Do this directly, not via WasmSimdFlag().
  //
  // Do not go via WasmSimdFlag() because we do not want to gate on
  // j.o.wasm_simd.  If the wormhole is available, requesting it will
  // force-enable SIMD.
  return js::jit::JitSupportsWasmSimd() &&
         (WasmSimdWormholeFlag(cx) || IsSimdPrivilegedContext(cx)) &&
         (IonAvailable(cx) || BaselineAvailable(cx)) && !CraneliftAvailable(cx);
#else
  return false;
#endif
}

bool wasm::ThreadsAvailable(JSContext* cx) {
  return WasmThreadsFlag(cx) && AnyCompilerAvailable(cx);
}

bool wasm::HasPlatformSupport(JSContext* cx) {
#if !MOZ_LITTLE_ENDIAN() || defined(JS_CODEGEN_NONE) || defined(__wasi__)
  return false;
#else

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

  if (!jit::JitSupportsAtomics()) {
    return false;
  }

  // Wasm threads require 8-byte lock-free atomics.
  if (!jit::AtomicOperations::isLockfree8()) {
    return false;
  }

  // Test only whether the compilers are supported on the hardware, not whether
  // they are enabled.
  return BaselinePlatformSupport() || IonPlatformSupport() ||
         CraneliftPlatformSupport();
#endif
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
  // Do not check for compiler availability, as that may be run-time variant.
  // For HasSupport() we want a stable answer depending only on prefs.
  return prefEnabled && HasPlatformSupport(cx);
}

bool wasm::StreamingCompilationAvailable(JSContext* cx) {
  // This should match EnsureStreamSupport().
  return HasSupport(cx) && AnyCompilerAvailable(cx) &&
         cx->runtime()->offThreadPromiseState.ref().initialized() &&
         CanUseExtraThreads() && cx->runtime()->consumeStreamCallback &&
         cx->runtime()->reportStreamErrorCallback;
}

bool wasm::CodeCachingAvailable(JSContext* cx) {
  // Fuzzilli breaks the out-of-process compilation mechanism,
  // so we disable it permanently in those builds.
#ifdef FUZZING_JS_FUZZILLI
  return false;
#else

  // At the moment, we require Ion support for code caching.  The main reason
  // for this is that wasm::CompileAndSerialize() does not have access to
  // information about which optimizing compiler it should use.  See comments in
  // CompileAndSerialize(), below.
  return StreamingCompilationAvailable(cx) && IonAvailable(cx);
#endif
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

#ifdef ENABLE_WASM_EXCEPTIONS
  uint32_t tagIndex = 0;
  const TagDescVector& tags = metadata.tags;
#endif
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
        if (obj->table().elemType() != tables[index].elemType) {
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
#ifdef ENABLE_WASM_EXCEPTIONS
      case DefinitionKind::Tag: {
        const uint32_t index = tagIndex++;
        if (!v.isObject() || !v.toObject().is<WasmTagObject>()) {
          return ThrowBadImportType(cx, import.field.get(), "Tag");
        }

        RootedWasmTagObject obj(cx, &v.toObject().as<WasmTagObject>());

        // Checks whether the signature of the imported exception object matches
        // the signature declared in the exception import's TagDesc.
        if (obj->resultType() != tags[index].resultType()) {
          JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                   JSMSG_WASM_BAD_TAG_SIG, import.module.get(),
                                   import.field.get());
          return false;
        }

        if (!imports->tagObjs.append(obj)) {
          ReportOutOfMemory(cx);
          return false;
        }
        break;
      }
#endif
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
          val = obj->val();
        } else {
          if (!global.type().isRefType()) {
            if (global.type() == ValType::I64 && !v.isBigInt()) {
              return ThrowBadImportType(cx, import.field.get(), "BigInt");
            }
            if (global.type() != ValType::I64 && !v.isNumber()) {
              return ThrowBadImportType(cx, import.field.get(), "Number");
            }
          } else {
            if (!global.type().isExternRef() && !v.isObjectOrNull()) {
              return ThrowBadImportType(cx, import.field.get(),
                                        "Object-or-null value required for "
                                        "non-externref reference type");
            }
          }

          if (global.isMutable()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                     JSMSG_WASM_BAD_GLOB_MUT_LINK);
            return false;
          }

          if (!Val::fromJSValue(cx, global.type(), v, &val)) {
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

// Parse the options bag that is optionally passed to functions that compile
// wasm.  This is for internal experimentation purposes.  See comments about the
// SIMD wormhole in WasmConstants.h.

static bool ParseCompileOptions(JSContext* cx, HandleValue maybeOptions,
                                FeatureOptions* options) {
  if (SimdWormholeAvailable(cx)) {
    if (maybeOptions.isObject()) {
      RootedValue wormholeVal(cx);
      RootedObject obj(cx, &maybeOptions.toObject());
      if (!JS_GetProperty(cx, obj, "simdWormhole", &wormholeVal)) {
        return false;
      }
      if (wormholeVal.isBoolean()) {
        options->simdWormhole = wormholeVal.toBoolean();
      }
    }
  }
  return true;
}

static SharedCompileArgs InitCompileArgs(JSContext* cx,
                                         HandleValue maybeOptions,
                                         const char* introducer) {
  ScriptedCaller scriptedCaller;
  if (!DescribeScriptedCaller(cx, &scriptedCaller, introducer)) {
    return nullptr;
  }

  FeatureOptions options;
  if (!ParseCompileOptions(cx, maybeOptions, &options)) {
    return nullptr;
  }
  return CompileArgs::build(cx, std::move(scriptedCaller), options);
}

// ============================================================================
// Testing / Fuzzing support

bool wasm::Eval(JSContext* cx, Handle<TypedArrayObject*> code,
                HandleObject importObj, HandleValue maybeOptions,
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

  SharedCompileArgs compileArgs =
      InitCompileArgs(cx, maybeOptions, "wasm_eval");
  if (!compileArgs) {
    return false;
  }

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, *bytecode, &error, &warnings, nullptr);
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

  void storeOptimizedEncoding(const uint8_t* bytes, size_t length) override {
    MOZ_ASSERT(!called);
    called = true;
    if (serialized->resizeUninitialized(length)) {
      memcpy(serialized->begin(), bytes, length);
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
//
// This just generalizes that to an arbitrary limit that is representable as an
// integer in double form.

static bool EnforceRange(JSContext* cx, HandleValue v, const char* kind,
                         const char* noun, uint64_t max, uint64_t* val) {
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
  if (x < 0 || x > double(max)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_UINT32, kind, noun);
    return false;
  }

  *val = uint64_t(x);
  MOZ_ASSERT(double(*val) == x);
  return true;
}

static bool EnforceRangeU32(JSContext* cx, HandleValue v, const char* kind,
                            const char* noun, uint32_t* u32) {
  uint64_t u64 = 0;
  if (!EnforceRange(cx, v, kind, noun, uint64_t(UINT32_MAX), &u64)) {
    return false;
  }
  *u32 = uint32_t(u64);
  return true;
}

static bool GetLimit(JSContext* cx, HandleObject obj, const char* name,
                     const char* noun, const char* msg, uint32_t range,
                     bool* found, uint64_t* value) {
  JSAtom* atom = Atomize(cx, name, strlen(name));
  if (!atom) {
    return false;
  }
  RootedId id(cx, AtomToId(atom));

  RootedValue val(cx);
  if (!GetProperty(cx, obj, obj, id, &val)) {
    return false;
  }

  if (val.isUndefined()) {
    *found = false;
    return true;
  }
  *found = true;
  // The range can be greater than 53, but then the logic in EnforceRange has to
  // change to avoid precision loss.
  MOZ_ASSERT(range < 54);
  return EnforceRange(cx, val, noun, msg, (uint64_t(1) << range) - 1, value);
}

static bool GetLimits(JSContext* cx, HandleObject obj, LimitsKind kind,
                      Limits* limits) {
  limits->indexType = IndexType::I32;

  // Memory limits may specify an alternate index type, and we need this to
  // check the ranges for initial and maximum, so look for the index type first.
  if (kind == LimitsKind::Memory) {
#ifdef ENABLE_WASM_MEMORY64
    // Get the index type field
    JSAtom* indexTypeAtom = Atomize(cx, "index", strlen("index"));
    if (!indexTypeAtom) {
      return false;
    }
    RootedId indexTypeId(cx, AtomToId(indexTypeAtom));

    RootedValue indexTypeVal(cx);
    if (!GetProperty(cx, obj, obj, indexTypeId, &indexTypeVal)) {
      return false;
    }

    // The index type has a default value
    if (!indexTypeVal.isUndefined()) {
      if (!ToIndexType(cx, indexTypeVal, &limits->indexType)) {
        return false;
      }

      if (limits->indexType == IndexType::I64 && !Memory64Available(cx)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_WASM_NO_MEM64_LINK);
        return false;
      }
    }
#endif
  }

  const char* noun = (kind == LimitsKind::Memory ? "Memory" : "Table");
  // 2^48 is a valid value, so the range goes to 49 bits.  Values above 2^48 are
  // filtered later, just as values above 2^16 are filtered for mem32.
  const uint32_t range = limits->indexType == IndexType::I32 ? 32 : 49;
  uint64_t limit = 0;

  bool haveInitial = false;
  if (!GetLimit(cx, obj, "initial", noun, "initial size", range, &haveInitial,
                &limit)) {
    return false;
  }
  if (haveInitial) {
    limits->initial = limit;
  }

  bool haveMinimum = false;
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
  if (!GetLimit(cx, obj, "minimum", noun, "initial size", range, &haveMinimum,
                &limit)) {
    return false;
  }
  if (haveMinimum) {
    limits->initial = limit;
  }
#endif

  if (!(haveInitial || haveMinimum)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_MISSING_REQUIRED, "initial");
    return false;
  }
  if (haveInitial && haveMinimum) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_SUPPLY_ONLY_ONE, "minimum", "initial");
    return false;
  }

  bool haveMaximum = false;
  if (!GetLimit(cx, obj, "maximum", noun, "maximum size", range, &haveMaximum,
                &limit)) {
    return false;
  }
  if (haveMaximum) {
    limits->maximum = Some(limit);
  }

  limits->shared = Shareable::False;

  // Memory limits may be shared.
  if (kind == LimitsKind::Memory) {
    // Get the shared field
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
        if (!haveMaximum) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_WASM_MISSING_MAXIMUM, noun);
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

static bool CheckLimits(JSContext* cx, uint64_t maximumField, LimitsKind kind,
                        Limits* limits) {
  const char* noun = (kind == LimitsKind::Memory ? "Memory" : "Table");

  if (limits->initial > maximumField) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_RANGE,
                             noun, "initial size");
    return false;
  }

  if (limits->maximum.isSome() &&
      (*limits->maximum > maximumField || limits->initial > *limits->maximum)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_RANGE,
                             noun, "maximum size");
    return false;
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

static JSObject* GetWasmConstructorPrototype(JSContext* cx,
                                             const CallArgs& callArgs,
                                             JSProtoKey key) {
  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, callArgs, key, &proto)) {
    return nullptr;
  }
  if (!proto) {
    proto = GlobalObject::getOrCreatePrototype(cx, key);
  }
  return proto;
}

static JSString* UTF8CharsToString(JSContext* cx, const char* chars) {
  return NewStringCopyUTF8Z<CanGC>(cx,
                                   JS::ConstUTF8CharsZ(chars, strlen(chars)));
}

[[nodiscard]] static bool ParseValTypes(JSContext* cx, HandleValue src,
                                        ValTypeVector& dest) {
  JS::ForOfIterator iterator(cx);

  if (!iterator.init(src, JS::ForOfIterator::ThrowOnNonIterable)) {
    return false;
  }

  RootedValue nextParam(cx);
  while (true) {
    bool done;
    if (!iterator.next(&nextParam, &done)) {
      return false;
    }
    if (done) {
      break;
    }

    ValType valType;
    if (!ToValType(cx, nextParam, &valType) || !dest.append(valType)) {
      return false;
    }
  }
  return true;
}

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
[[nodiscard]] static JSObject* ValTypesToArray(JSContext* cx,
                                               const ValTypeVector& valTypes) {
  RootedArrayObject arrayObj(cx, NewDenseEmptyArray(cx));
  for (ValType valType : valTypes) {
    RootedString type(cx, UTF8CharsToString(cx, ToJSAPIString(valType).get()));
    if (!type) {
      return nullptr;
    }
    if (!NewbornArrayPush(cx, arrayObj, StringValue(type))) {
      return nullptr;
    }
  }
  return arrayObj;
}

static JSObject* FuncTypeToObject(JSContext* cx, const FuncType& type) {
  Rooted<IdValueVector> props(cx, IdValueVector(cx));

  RootedObject parametersObj(cx, ValTypesToArray(cx, type.args()));
  if (!parametersObj ||
      !props.append(IdValuePair(NameToId(cx->names().parameters),
                                ObjectValue(*parametersObj)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  RootedObject resultsObj(cx, ValTypesToArray(cx, type.results()));
  if (!resultsObj || !props.append(IdValuePair(NameToId(cx->names().results),
                                               ObjectValue(*resultsObj)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return NewPlainObjectWithProperties(cx, props.begin(), props.length(),
                                      GenericObject);
}

static JSObject* TableTypeToObject(JSContext* cx, RefType type,
                                   uint32_t initial, Maybe<uint32_t> maximum) {
  Rooted<IdValueVector> props(cx, IdValueVector(cx));

  RootedString elementType(cx,
                           UTF8CharsToString(cx, ToJSAPIString(type).get()));
  if (!elementType || !props.append(IdValuePair(NameToId(cx->names().element),
                                                StringValue(elementType)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  if (maximum.isSome()) {
    if (!props.append(IdValuePair(NameToId(cx->names().maximum),
                                  Int32Value(maximum.value())))) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
  }

  if (!props.append(
          IdValuePair(NameToId(cx->names().minimum), Int32Value(initial)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return NewPlainObjectWithProperties(cx, props.begin(), props.length(),
                                      GenericObject);
}

static JSObject* MemoryTypeToObject(JSContext* cx, bool shared,
                                    wasm::IndexType indexType,
                                    wasm::Pages minPages,
                                    Maybe<wasm::Pages> maxPages) {
  Rooted<IdValueVector> props(cx, IdValueVector(cx));
  if (maxPages) {
    double maxPagesNum;
    if (indexType == IndexType::I32) {
      maxPagesNum = double(mozilla::AssertedCast<uint32_t>(maxPages->value()));
    } else {
      // The maximum number of pages is 2^48.
      maxPagesNum = double(maxPages->value());
    }
    if (!props.append(IdValuePair(NameToId(cx->names().maximum),
                                  NumberValue(maxPagesNum)))) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
  }

  double minPagesNum;
  if (indexType == IndexType::I32) {
    minPagesNum = double(mozilla::AssertedCast<uint32_t>(minPages.value()));
  } else {
    minPagesNum = double(minPages.value());
  }
  if (!props.append(IdValuePair(NameToId(cx->names().minimum),
                                NumberValue(minPagesNum)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

#  ifdef ENABLE_WASM_MEMORY64
  RootedString it(
      cx, JS_NewStringCopyZ(cx, indexType == IndexType::I32 ? "i32" : "i64"));
  if (!props.append(
          IdValuePair(NameToId(cx->names().index), StringValue(it)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
#  endif

  if (!props.append(
          IdValuePair(NameToId(cx->names().shared), BooleanValue(shared)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return NewPlainObjectWithProperties(cx, props.begin(), props.length(),
                                      GenericObject);
}

static JSObject* GlobalTypeToObject(JSContext* cx, ValType type,
                                    bool isMutable) {
  Rooted<IdValueVector> props(cx, IdValueVector(cx));

  if (!props.append(IdValuePair(NameToId(cx->names().mutable_),
                                BooleanValue(isMutable)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  RootedString valueType(cx, UTF8CharsToString(cx, ToJSAPIString(type).get()));
  if (!valueType || !props.append(IdValuePair(NameToId(cx->names().value),
                                              StringValue(valueType)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return NewPlainObjectWithProperties(cx, props.begin(), props.length(),
                                      GenericObject);
}

#  ifdef ENABLE_WASM_EXCEPTIONS
static JSObject* TagTypeToObject(JSContext* cx,
                                 const wasm::ValTypeVector& params) {
  Rooted<IdValueVector> props(cx, IdValueVector(cx));

  RootedObject parametersObj(cx, ValTypesToArray(cx, params));
  if (!parametersObj ||
      !props.append(IdValuePair(NameToId(cx->names().parameters),
                                ObjectValue(*parametersObj)))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return NewPlainObjectWithProperties(cx, props.begin(), props.length(),
                                      GenericObject);
}
#  endif  // ENABLE_WASM_EXCEPTIONS

#endif  // ENABLE_WASM_TYPE_REFLECTIONS

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
  RootedPropertyName tag;
  RootedPropertyName type;

  explicit KindNames(JSContext* cx)
      : kind(cx), table(cx), memory(cx), tag(cx), type(cx) {}
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

#ifdef ENABLE_WASM_EXCEPTIONS
  JSAtom* tag = Atomize(cx, "tag", strlen("tag"));
  if (!tag) {
    return false;
  }
  names->tag = tag->asPropertyName();
#endif

  JSAtom* type = Atomize(cx, "type", strlen("type"));
  if (!type) {
    return false;
  }
  names->type = type->asPropertyName();

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
#ifdef ENABLE_WASM_EXCEPTIONS
    case DefinitionKind::Tag:
      return names.tag;
#endif
  }

  MOZ_CRASH("invalid kind");
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

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
  const Metadata& metadata = module->metadata();
  const MetadataTier& metadataTier =
      module->metadata(module->code().stableTier());

  size_t numFuncImport = 0;
  size_t numMemoryImport = 0;
  size_t numGlobalImport = 0;
  size_t numTableImport = 0;
#  ifdef ENABLE_WASM_EXCEPTIONS
  size_t numTagImport = 0;
#  endif  // ENABLE_WASM_EXCEPTIONS
#endif    // ENABLE_WASM_TYPE_REFLECTIONS

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

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    RootedObject typeObj(cx);
    switch (import.kind) {
      case DefinitionKind::Function: {
        size_t funcIndex = numFuncImport++;
        typeObj = FuncTypeToObject(
            cx, metadataTier.funcImports[funcIndex].funcType());
        break;
      }
      case DefinitionKind::Table: {
        size_t tableIndex = numTableImport++;
        const TableDesc& table = metadata.tables[tableIndex];
        typeObj = TableTypeToObject(cx, table.elemType, table.initialLength,
                                    table.maximumLength);
        break;
      }
      case DefinitionKind::Memory: {
        DebugOnly<size_t> memoryIndex = numMemoryImport++;
        MOZ_ASSERT(memoryIndex == 0);
        const MemoryDesc& memory = *metadata.memory;
        typeObj =
            MemoryTypeToObject(cx, memory.isShared(), memory.indexType(),
                               memory.initialPages(), memory.maximumPages());
        break;
      }
      case DefinitionKind::Global: {
        size_t globalIndex = numGlobalImport++;
        const GlobalDesc& global = metadata.globals[globalIndex];
        typeObj = GlobalTypeToObject(cx, global.type(), global.isMutable());
        break;
      }
#  ifdef ENABLE_WASM_EXCEPTIONS
      case DefinitionKind::Tag: {
        size_t tagIndex = numTagImport++;
        const TagDesc& tag = metadata.tags[tagIndex];
        typeObj = TagTypeToObject(cx, tag.type.argTypes);
        break;
      }
#  endif  // ENABLE_WASM_EXCEPTIONS
    }

    if (!typeObj || !props.append(IdValuePair(NameToId(names.type),
                                              ObjectValue(*typeObj)))) {
      ReportOutOfMemory(cx);
      return false;
    }
#endif  // ENABLE_WASM_TYPE_REFLECTIONS

    JSObject* obj = NewPlainObjectWithProperties(cx, props.begin(),
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

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
  const Metadata& metadata = module->metadata();
  const MetadataTier& metadataTier =
      module->metadata(module->code().stableTier());
#endif  // ENABLE_WASM_TYPE_REFLECTIONS

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

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    RootedObject typeObj(cx);
    switch (exp.kind()) {
      case DefinitionKind::Function: {
        const FuncExport& fe = metadataTier.lookupFuncExport(exp.funcIndex());
        typeObj = FuncTypeToObject(cx, fe.funcType());
        break;
      }
      case DefinitionKind::Table: {
        const TableDesc& table = metadata.tables[exp.tableIndex()];
        typeObj = TableTypeToObject(cx, table.elemType, table.initialLength,
                                    table.maximumLength);
        break;
      }
      case DefinitionKind::Memory: {
        const MemoryDesc& memory = *metadata.memory;
        typeObj =
            MemoryTypeToObject(cx, memory.isShared(), memory.indexType(),
                               memory.initialPages(), memory.maximumPages());
        break;
      }
      case DefinitionKind::Global: {
        const GlobalDesc& global = metadata.globals[exp.globalIndex()];
        typeObj = GlobalTypeToObject(cx, global.type(), global.isMutable());
        break;
      }
#  ifdef ENABLE_WASM_EXCEPTIONS
      case DefinitionKind::Tag: {
        const TagDesc& tag = metadata.tags[exp.tagIndex()];
        typeObj = TagTypeToObject(cx, tag.type.argTypes);
        break;
      }
#  endif  // ENABLE_WASM_EXCEPTIONS
    }

    if (!typeObj || !props.append(IdValuePair(NameToId(names.type),
                                              ObjectValue(*typeObj)))) {
      ReportOutOfMemory(cx);
      return false;
    }
#endif  // ENABLE_WASM_TYPE_REFLECTIONS

    JSObject* obj = NewPlainObjectWithProperties(cx, props.begin(),
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

    (void)JS::DeflateStringToUTF8Buffer(linear,
                                        Span(name.begin(), name.length()));
  }

  RootedValueVector elems(cx);
  RootedArrayBufferObject buf(cx);
  for (const CustomSection& cs : module->customSections()) {
    if (name.length() != cs.name.length()) {
      continue;
    }
    if (memcmp(name.begin(), cs.name.begin(), name.length()) != 0) {
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

  SharedCompileArgs compileArgs =
      InitCompileArgs(cx, callArgs.get(1), "WebAssembly.Module");
  if (!compileArgs) {
    return false;
  }

  UniqueChars error;
  UniqueCharsVector warnings;
  SharedModule module =
      CompileBuffer(*compileArgs, *bytecode, &error, &warnings, nullptr);

  if (!ReportCompileWarnings(cx, warnings)) {
    return false;
  }
  if (!module) {
    if (error) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_COMPILE_ERROR, error.get());
      return false;
    }
    ReportOutOfMemory(cx);
    return false;
  }

  RootedObject proto(
      cx, GetWasmConstructorPrototype(cx, callArgs, JSProto_WasmModule));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }

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

// WeakScopeMap maps from function index to js::Scope. This maps is weak
// to avoid holding scope objects alive. The scopes are normally created
// during debugging.
//
// This is defined here in order to avoid recursive dependency between
// WasmJS.h and Scope.h.
using WasmFunctionScopeMap =
    JS::WeakCache<GCHashMap<uint32_t, WeakHeapPtr<WasmFunctionScope*>,
                            DefaultHasher<uint32_t>, ZoneAllocPolicy>>;
class WasmInstanceObject::UnspecifiedScopeMap {
 public:
  WasmFunctionScopeMap& asWasmFunctionScopeMap() {
    return *(WasmFunctionScopeMap*)this;
  }
};

/* static */
void WasmInstanceObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmInstanceObject& instance = obj->as<WasmInstanceObject>();
  fop->delete_(obj, &instance.exports(), MemoryUse::WasmInstanceExports);
  fop->delete_(obj, &instance.scopes().asWasmFunctionScopeMap(),
               MemoryUse::WasmInstanceScopes);
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
    const ElemSegmentVector& elemSegments, uint32_t globalDataLength,
    HandleWasmMemoryObject memory, SharedExceptionTagVector&& exceptionTags,
    SharedTableVector&& tables, const JSFunctionVector& funcImports,
    const GlobalDescVector& globals, const ValVector& globalImportValues,
    const WasmGlobalObjectVector& globalObjs, HandleObject proto,
    UniqueDebugState maybeDebug) {
  Rooted<UniquePtr<ExportMap>> exports(cx,
                                       js::MakeUnique<ExportMap>(cx->zone()));
  if (!exports) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  UniquePtr<WasmFunctionScopeMap> scopes =
      js::MakeUnique<WasmFunctionScopeMap>(cx->zone(), cx->zone());
  if (!scopes) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
  // Note that `scopes` is a WeakCache, auto-linked into a sweep list on the
  // Zone, and so does not require rooting.

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

    // Create this just before constructing Instance to avoid rooting hazards.
    UniqueTlsData tlsData = CreateTlsData(globalDataLength);
    if (!tlsData) {
      ReportOutOfMemory(cx);
      return nullptr;
    }

    // Root the Instance via WasmInstanceObject before any possible GC.
    instance = cx->new_<Instance>(cx, obj, code, std::move(tlsData), memory,
                                  std::move(exceptionTags), std::move(tables),
                                  std::move(maybeDebug));
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

  RootedObject proto(
      cx, GetWasmConstructorPrototype(cx, args, JSProto_WasmInstance));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }

  Rooted<ImportValues> imports(cx);
  if (!GetImports(cx, *module, importObj, imports.address())) {
    return false;
  }

  RootedWasmInstanceObject instanceObj(cx);
  if (!module->instantiate(cx, imports.get(), proto, &instanceObj)) {
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

WasmInstanceObject::UnspecifiedScopeMap& WasmInstanceObject::scopes() const {
  return *(UnspecifiedScopeMap*)(getReservedSlot(SCOPES_SLOT).toPrivate());
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

/*
 * [SMDOC] Exported wasm functions and the jit-entry stubs
 *
 * ## The kinds of exported functions
 *
 * There are several kinds of exported wasm functions.  /Explicitly/ exported
 * functions are:
 *
 *  - any wasm function exported via the export section
 *  - any asm.js export
 *  - the module start function
 *
 * There are also /implicitly/ exported functions, these are the functions whose
 * indices in the module are referenced outside the code segment, eg, in element
 * segments and in global initializers.
 *
 * ## Wasm functions as JSFunctions
 *
 * Any exported function can be manipulated by JS and wasm code, and to both the
 * exported function is represented as a JSFunction.  To JS, that means that the
 * function can be called in the same way as any other JSFunction.  To Wasm, it
 * means that the function is a reference with the same representation as
 * externref.
 *
 * However, the JSFunction object is created only when the function value is
 * actually exposed to JS the first time.  The creation is performed by
 * getExportedFunction(), below, as follows:
 *
 *  - a function exported via the export section (or from asm.js) is created
 *    when the export object is created, which happens at instantiation time.
 *
 *  - a function implicitly exported via a table is created when the table
 *    element is read (by JS or wasm) and a function value is needed to
 *    represent that value.  Functions stored in tables by initializers have a
 *    special representation that does not require the function object to be
 *    created.
 *
 *  - a function implicitly exported via a global initializer is created when
 *    the global is initialized.
 *
 *  - a function referenced from a ref.func instruction in code is created when
 *    that instruction is executed the first time.
 *
 * The JSFunction representing a wasm function never changes: every reference to
 * the wasm function that exposes the JSFunction gets the same JSFunction.  In
 * particular, imported functions already have a JSFunction representation (from
 * JS or from their home module), and will be exposed using that representation.
 *
 * The mapping from a wasm function to its JSFunction is instance-specific, and
 * held in a hashmap in the instance.  If a module is shared across multiple
 * instances, possibly in multiple threads, each instance will have its own
 * JSFunction representing the wasm function.
 *
 * ## Stubs -- interpreter, eager, lazy, provisional, and absent
 *
 * While a Wasm exported function is just a JSFunction, the internal wasm ABI is
 * neither the C++ ABI nor the JS JIT ABI, so there needs to be an extra step
 * when C++ or JS JIT code calls wasm code.  For this, execution passes through
 * a stub that is adapted to both the JS caller and the wasm callee.
 *
 * ### Interpreter stubs and jit-entry stubs
 *
 * When JS interpreted code calls a wasm function, we end up in
 * Instance::callExport() to execute the call.  This function must enter wasm,
 * and to do this it uses a stub that is specific to the wasm function (see
 * GenerateInterpEntry) that is callable with the C++ interpreter ABI and which
 * will convert arguments as necessary and enter compiled wasm code.
 *
 * The interpreter stub is created eagerly, when the module is compiled.
 *
 * However, the interpreter call path is slow, and when JS jitted code calls
 * wasm we want to do better.  In this case, there is a different, optimized
 * stub that is to be invoked, and it uses the JIT ABI.  This is the jit-entry
 * stub for the function.  Jitted code will call a wasm function's jit-entry
 * stub to invoke the function with the JIT ABI.  The stub will adapt the call
 * to the wasm ABI.
 *
 * Some jit-entry stubs are created eagerly and some are created lazily.
 *
 * ### Eager jit-entry stubs
 *
 * The explicitly exported functions have stubs created for them eagerly.  Eager
 * stubs are created with their tier when the module is compiled, see
 * ModuleGenerator::finishCodeTier(), which calls wasm::GenerateStubs(), which
 * generates stubs for functions with eager stubs.
 *
 * An eager stub for tier-1 is upgraded to tier-2 if the module tiers up, see
 * below.
 *
 * ### Lazy jit-entry stubs
 *
 * Stubs are created lazily for all implicitly exported functions.  These
 * functions may flow out to JS, but will only need a stub if they are ever
 * called from jitted code.  (That's true for explicitly exported functions too,
 * but for them the presumption is that they will be called.)
 *
 * Lazy stubs are created only when they are needed, and they are /doubly/ lazy,
 * see getExportedFunction(), below: A function implicitly exported via a table
 * or global may be manipulated eagerly by host code without actually being
 * called (maybe ever), so we do not generate a lazy stub when the function
 * object escapes to JS, but instead delay stub generation until the function is
 * actually called.
 *
 * ### The provisional lazy jit-entry stub
 *
 * However, JS baseline compilation needs to have a stub to start with in order
 * to allow it to attach CacheIR data to the call (or it deoptimizes the call as
 * a C++ call).  Thus when the JSFunction for the wasm export is retrieved by JS
 * code, a /provisional/ lazy jit-entry stub is associated with the function.
 * The stub will invoke the wasm function on the slow interpreter path via
 * callExport - if the function is ever called - and will cause a fast jit-entry
 * stub to be created at the time of the call.  The provisional lazy stub is
 * shared globally, it contains no function-specific or context-specific data.
 *
 * Thus, the final lazy jit-entry stubs are eventually created by
 * Instance::callExport, when a call is routed through it on the slow path for
 * any of the reasons given above.
 *
 * ### Absent jit-entry stubs
 *
 * Some functions never get jit-entry stubs.  The predicate canHaveJitEntry()
 * determines if a wasm function gets a stub, and it will deny this if the
 * function's signature exposes non-JS-compatible types (such as v128) or if
 * stub optimization has been disabled by a jit option.  Calls to these
 * functions will continue to go via callExport and use the slow interpreter
 * stub.
 *
 * ## The jit-entry jump table
 *
 * The mapping from the exported function to its jit-entry stub is implemented
 * by the jit-entry jump table in the JumpTables object (see WasmCode.h).  The
 * jit-entry jump table entry for a function holds a stub that the jit can call
 * to perform fast calls.
 *
 * While there is a single contiguous jump table, it has two logical sections:
 * one for eager stubs, and one for lazy stubs.  These sections are initialized
 * and updated separately, using logic that is specific to each section.
 *
 * The value of the table element for an eager stub is a pointer to the stub
 * code in the current tier.  The pointer is installed just after the creation
 * of the stub, before any code in the module is executed.  If the module later
 * tiers up, the eager jit-entry stub for tier-1 code is replaced by one for
 * tier-2 code, see the next section.
 *
 * Initially the value of the jump table element for a lazy stub is null.
 *
 * If the function is retrieved by JS (by getExportedFunction()) and is not
 * barred from having a jit-entry, then the stub is upgraded to the shared
 * provisional lazy jit-entry stub.  This upgrade happens to be racy if the
 * module is shared, and so the update is atomic and only happens if the entry
 * is already null.  Since the provisional lazy stub is shared, this is fine; if
 * several threads try to upgrade at the same time, it is to the same shared
 * value.
 *
 * If the retrieved function is later invoked (via callExport()), the stub is
 * upgraded to an actual jit-entry stub for the current code tier, again if the
 * function is allowed to have a jit-entry.  This is not racy -- though multiple
 * threads can be trying to create a jit-entry stub at the same time, they do so
 * under a lock and only the first to take the lock will be allowed to create a
 * stub, the others will reuse the first-installed stub.
 *
 * If the module later tiers up, the lazy jit-entry stub for tier-1 code (if it
 * exists) is replaced by one for tier-2 code, see the next section.
 *
 * (Note, the InterpEntry stub is never stored in the jit-entry table, as it
 * uses the C++ ABI, not the JIT ABI.  It is accessible through the
 * FunctionEntry.)
 *
 * ### Interaction of the jit-entry jump table and tiering
 *
 * (For general info about tiering, see the comment in WasmCompile.cpp.)
 *
 * The jit-entry stub, whether eager or lazy, is specific to a code tier - a
 * stub will invoke the code for its function for the tier.  When we tier up,
 * new jit-entry stubs must be created that reference tier-2 code, and must then
 * be patched into the jit-entry table.  The complication here is that, since
 * the jump table is shared with its code between instances on multiple threads,
 * tier-1 code is running on other threads and new tier-1 specific jit-entry
 * stubs may be created concurrently with trying to create the tier-2 stubs on
 * the thread that performs the tiering-up.  Indeed, there may also be
 * concurrent attempts to upgrade null jit-entries to the provisional lazy stub.
 *
 * Eager stubs:
 *
 *  - Eager stubs for tier-2 code are patched in racily by Module::finishTier2()
 *    along with code pointers for tiering; nothing conflicts with these writes.
 *
 * Lazy stubs:
 *
 *  - An upgrade from a null entry to a lazy provisional stub is atomic and can
 *    only happen if the entry is null, and it only happens in
 *    getExportedFunction().  No lazy provisional stub will be installed if
 *    there's another stub present.
 *
 *  - The lazy tier-appropriate stub is installed by callExport() (really by
 *    EnsureEntryStubs()) during the first invocation of the exported function
 *    that reaches callExport().  That invocation must be from within JS, and so
 *    the jit-entry element can't be null, because a prior getExportedFunction()
 *    will have ensured that it is not: the lazy provisional stub will have been
 *    installed.  Hence the installing of the lazy tier-appropriate stub does
 *    not race with the installing of the lazy provisional stub.
 *
 *  - A lazy tier-1 stub is upgraded to a lazy tier-2 stub by
 *    Module::finishTier2().  The upgrade needs to ensure that all tier-1 stubs
 *    are upgraded, and that once the upgrade is finished, callExport() will
 *    only create tier-2 lazy stubs.  (This upgrading does not upgrade lazy
 *    provisional stubs or absent stubs.)
 *
 *    The locking protocol ensuring that all stubs are upgraded properly and
 *    that the system switches to creating tier-2 stubs is implemented in
 *    Module::finishTier2() and EnsureEntryStubs():
 *
 *    There are two locks, one per code tier.
 *
 *    EnsureEntryStubs() is attempting to create a tier-appropriate lazy stub,
 *    so it takes the lock for the current best tier, checks to see if there is
 *    a stub, and exits if there is.  If the tier changed racily it takes the
 *    other lock too, since that is now the lock for the best tier.  Then it
 *    creates the stub, installs it, and releases the locks.  Thus at most one
 *    stub per tier can be created at a time.
 *
 *    Module::finishTier2() takes both locks (tier-1 before tier-2), thus
 *    preventing EnsureEntryStubs() from creating stubs while stub upgrading is
 *    going on, and itself waiting until EnsureEntryStubs() is not active.  Once
 *    it has both locks, it upgrades all lazy stubs and makes tier-2 the new
 *    best tier.  Should EnsureEntryStubs subsequently enter, it will find that
 *    a stub already exists at tier-2 and will exit early.
 *
 * (It would seem that the locking protocol could be simplified a little by
 * having only one lock, hanging off the Code object, or by unconditionally
 * taking both locks in EnsureEntryStubs().  However, in some cases where we
 * acquire a lock the Code object is not readily available, so plumbing would
 * have to be added, and in EnsureEntryStubs(), there are sometimes not two code
 * tiers.)
 *
 * ## Stub lifetimes and serialization
 *
 * Eager jit-entry stub code, along with stub code for import functions, is
 * serialized along with the tier-2 code for the module.
 *
 * Lazy stub code and thunks for builtin functions (including the provisional
 * lazy jit-entry stub) are never serialized.
 */

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
                                 TenuredObject, FunctionFlags::ASMJS_CTOR));
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
    RootedObject proto(cx);
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    proto = GlobalObject::getOrCreatePrototype(cx, JSProto_WasmFunction);
    if (!proto) {
      return false;
    }
#endif
    fun.set(NewFunctionWithProto(
        cx, WasmCall, numArgs, FunctionFlags::WASM, nullptr, name, proto,
        gc::AllocKind::FUNCTION_EXTENDED, TenuredObject));
    if (!fun) {
      return false;
    }

    // Some applications eagerly access all table elements which currently
    // triggers worst-case behavior for lazy stubs, since each will allocate a
    // separate 4kb code page. Most eagerly-accessed functions are not called,
    // so use a shared, provisional (and slow) lazy stub as JitEntry and wait
    // until Instance::callExport() to create the fast entry stubs.
    if (funcExport.canHaveJitEntry()) {
      if (!funcExport.hasEagerStubs()) {
        if (!EnsureBuiltinThunksInitialized()) {
          return false;
        }
        void* provisionalLazyJitEntryStub = ProvisionalLazyJitEntryStub();
        MOZ_ASSERT(provisionalLazyJitEntryStub);
        instance.code().setJitEntryIfNull(funcIndex,
                                          provisionalLazyJitEntryStub);
      }
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
  if (auto p =
          instanceObj->scopes().asWasmFunctionScopeMap().lookup(funcIndex)) {
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

  if (!instanceObj->scopes().asWasmFunctionScopeMap().putNew(funcIndex,
                                                             funcScope)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return funcScope;
}

bool wasm::IsWasmExportedFunction(JSFunction* fun) {
  return fun->kind() == FunctionFlags::Wasm;
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
    JSContext* cx, HandleArrayBufferObjectMaybeShared buffer, bool isHuge,
    HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  auto* obj = NewObjectWithGivenProto<WasmMemoryObject>(cx, proto);
  if (!obj) {
    return nullptr;
  }

  obj->initReservedSlot(BUFFER_SLOT, ObjectValue(*buffer));
  obj->initReservedSlot(ISHUGE_SLOT, BooleanValue(isHuge));
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
  if (!GetLimits(cx, obj, LimitsKind::Memory, &limits) ||
      !CheckLimits(cx, MaxMemoryLimitField(limits.indexType),
                   LimitsKind::Memory, &limits)) {
    return false;
  }

  if (Pages(limits.initial) > MaxMemoryPages(limits.indexType)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_MEM_IMP_LIMIT);
    return false;
  }
  MemoryDesc memory(limits);

  RootedArrayBufferObjectMaybeShared buffer(cx);
  if (!CreateWasmBuffer(cx, memory, &buffer)) {
    return false;
  }

  RootedObject proto(cx,
                     GetWasmConstructorPrototype(cx, args, JSProto_WasmMemory));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }

  RootedWasmMemoryObject memoryObj(
      cx, WasmMemoryObject::create(
              cx, buffer, IsHugeMemoryEnabled(limits.indexType), proto));
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
    size_t memoryLength = memoryObj->volatileMemoryLength();
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
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    JS_FN("type", WasmMemoryObject::type, 0, JSPROP_ENUMERATE),
#endif
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

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
bool WasmMemoryObject::typeImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmMemoryObject memoryObj(
      cx, &args.thisv().toObject().as<WasmMemoryObject>());
  RootedObject typeObj(
      cx, MemoryTypeToObject(cx, memoryObj->isShared(), memoryObj->indexType(),
                             memoryObj->volatilePages(),
                             memoryObj->sourceMaxPages()));
  if (!typeObj) {
    return false;
  }
  args.rval().setObject(*typeObj);
  return true;
}

bool WasmMemoryObject::type(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsMemory, typeImpl>(cx, args);
}
#endif

size_t WasmMemoryObject::volatileMemoryLength() const {
  if (isShared()) {
    return sharedArrayRawBuffer()->volatileByteLength();
  }
  return buffer().byteLength();
}

wasm::Pages WasmMemoryObject::volatilePages() const {
  if (isShared()) {
    return sharedArrayRawBuffer()->volatileWasmPages();
  }
  return buffer().wasmPages();
}

wasm::Pages WasmMemoryObject::clampedMaxPages() const {
  if (isShared()) {
    return sharedArrayRawBuffer()->wasmClampedMaxPages();
  }
  return buffer().wasmClampedMaxPages();
}

Maybe<wasm::Pages> WasmMemoryObject::sourceMaxPages() const {
  if (isShared()) {
    return Some(sharedArrayRawBuffer()->wasmSourceMaxPages());
  }
  return buffer().wasmSourceMaxPages();
}

wasm::IndexType WasmMemoryObject::indexType() const {
  if (isShared()) {
    return sharedArrayRawBuffer()->wasmIndexType();
  }
  return buffer().wasmIndexType();
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
  return getReservedSlot(ISHUGE_SLOT).toBoolean();
}

bool WasmMemoryObject::movingGrowable() const {
  return !isHuge() && !buffer().wasmSourceMaxPages();
}

size_t WasmMemoryObject::boundsCheckLimit() const {
  if (!buffer().isWasm() || isHuge()) {
    return buffer().byteLength();
  }
  size_t mappedSize = buffer().wasmMappedSize();
#if !defined(JS_64BIT) || defined(ENABLE_WASM_CRANELIFT)
  // See clamping performed in CreateSpecificWasmBuffer().  On 32-bit systems
  // and on 64-bit with Cranelift, we do not want to overflow a uint32_t.  For
  // the other 64-bit compilers, all constraints are implied by the largest
  // accepted value for a memory's max field.
  MOZ_ASSERT(mappedSize < UINT32_MAX);
#endif
  MOZ_ASSERT(mappedSize % wasm::PageSize == 0);
  MOZ_ASSERT(mappedSize >= wasm::GuardSize);
  MOZ_ASSERT(wasm::IsValidBoundsCheckImmediate(mappedSize - wasm::GuardSize));
  size_t limit = mappedSize - wasm::GuardSize;
  MOZ_ASSERT(limit <= MaxMemoryBoundsCheckLimit(indexType()));
  return limit;
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
uint64_t WasmMemoryObject::growShared(HandleWasmMemoryObject memory,
                                      uint64_t delta) {
  SharedArrayRawBuffer* rawBuf = memory->sharedArrayRawBuffer();
  SharedArrayRawBuffer::Lock lock(rawBuf);

  Pages oldNumPages = rawBuf->volatileWasmPages();
  Pages newPages = oldNumPages;
  if (!newPages.checkedIncrement(Pages(delta))) {
    return uint64_t(int64_t(-1));
  }

  if (!rawBuf->wasmGrowToPagesInPlace(lock, memory->indexType(), newPages)) {
    return uint64_t(int64_t(-1));
  }
  // New buffer objects will be created lazily in all agents (including in
  // this agent) by bufferGetterImpl, above, so no more work to do here.

  return oldNumPages.value();
}

/* static */
uint64_t WasmMemoryObject::grow(HandleWasmMemoryObject memory, uint64_t delta,
                                JSContext* cx) {
  if (memory->isShared()) {
    return growShared(memory, delta);
  }

  RootedArrayBufferObject oldBuf(cx, &memory->buffer().as<ArrayBufferObject>());

#if !defined(JS_64BIT) || defined(ENABLE_WASM_CRANELIFT)
  // TODO (large ArrayBuffer): For Cranelift, limit the memory size to something
  // that fits in a uint32_t.  See more information at the definition of
  // MaxMemoryBytes().
  MOZ_ASSERT(MaxMemoryBytes(memory->indexType()) <= UINT32_MAX,
             "Avoid 32-bit overflows");
#endif

  Pages oldNumPages = oldBuf->wasmPages();
  Pages newPages = oldNumPages;
  if (!newPages.checkedIncrement(Pages(delta))) {
    return uint64_t(int64_t(-1));
  }

  RootedArrayBufferObject newBuf(cx);

  if (memory->movingGrowable()) {
    MOZ_ASSERT(!memory->isHuge());
    if (!ArrayBufferObject::wasmMovingGrowToPages(memory->indexType(), newPages,
                                                  oldBuf, &newBuf, cx)) {
      return uint64_t(int64_t(-1));
    }
  } else if (!ArrayBufferObject::wasmGrowToPagesInPlace(
                 memory->indexType(), newPages, oldBuf, &newBuf, cx)) {
    return uint64_t(int64_t(-1));
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

  return oldNumPages.value();
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

// Return the JS value to use when a parameter to a function requiring a table
// value is omitted. An implementation of [1].
//
// [1]
// https://webassembly.github.io/reference-types/js-api/index.html#defaultvalue
static Value TableDefaultValue(wasm::RefType tableType) {
  return tableType.isExtern() ? UndefinedValue() : NullValue();
}

/* static */
WasmTableObject* WasmTableObject::create(JSContext* cx, uint32_t initialLength,
                                         Maybe<uint32_t> maximumLength,
                                         wasm::RefType tableType,
                                         HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmTableObject obj(
      cx, NewObjectWithGivenProto<WasmTableObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());

  TableDesc td(tableType, initialLength, maximumLength, /*isAsmJS*/ false,
               /*importedOrExported=*/true);

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

  RefType tableType;
  if (!ToRefType(cx, elementLinearStr, &tableType)) {
    return false;
  }

  Limits limits;
  if (!GetLimits(cx, obj, LimitsKind::Table, &limits) ||
      !CheckLimits(cx, MaxTableLimitField, LimitsKind::Table, &limits)) {
    return false;
  }

  // Converting limits for a table only supports i32
  MOZ_ASSERT(limits.indexType == IndexType::I32);

  if (limits.initial > MaxTableLength) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_TABLE_IMP_LIMIT);
    return false;
  }

  RootedObject proto(cx,
                     GetWasmConstructorPrototype(cx, args, JSProto_WasmTable));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }

  // The rest of the runtime expects table limits to be within a 32-bit range.
  static_assert(MaxTableLimitField <= UINT32_MAX, "invariant");
  uint32_t initialLength = uint32_t(limits.initial);
  Maybe<uint32_t> maximumLength;
  if (limits.maximum) {
    maximumLength = Some(uint32_t(*limits.maximum));
  }

  RootedWasmTableObject table(
      cx, WasmTableObject::create(cx, initialLength, maximumLength, tableType,
                                  proto));
  if (!table) {
    return false;
  }

  // Initialize the table to a default value
  RootedValue initValue(
      cx, args.length() < 2 ? TableDefaultValue(tableType) : args[1]);

  // Skip initializing the table if the fill value is null, as that is the
  // default value.
  if (!initValue.isNull() &&
      !table->fillRange(cx, 0, initialLength, initValue)) {
    return false;
  }
#ifdef DEBUG
  // Assert that null is the default value of a new table.
  if (initValue.isNull()) {
    table->assertRangeNull(0, initialLength);
  }
#endif

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

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
/* static */
bool WasmTableObject::typeImpl(JSContext* cx, const CallArgs& args) {
  Table& table = args.thisv().toObject().as<WasmTableObject>().table();
  RootedObject typeObj(cx, TableTypeToObject(cx, table.elemType(),
                                             table.length(), table.maximum()));
  if (!typeObj) {
    return false;
  }
  args.rval().setObject(*typeObj);
  return true;
}

/* static */
bool WasmTableObject::type(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTable, typeImpl>(cx, args);
}
#endif

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
      MOZ_RELEASE_ASSERT(!table.isAsmJS());
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

  if (!args.requireAtLeast(cx, "WebAssembly.Table.set", 1)) {
    return false;
  }

  uint32_t index;
  if (!ToTableIndex(cx, args.get(0), table, "set index", &index)) {
    return false;
  }

  RootedValue fillValue(
      cx, args.length() < 2 ? TableDefaultValue(table.elemType()) : args[1]);
  if (!tableObj->fillRange(cx, index, 1, fillValue)) {
    return false;
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

  // Fill the grown range of the table
  RootedValue fillValue(
      cx, args.length() < 2 ? TableDefaultValue(table.elemType()) : args[1]);

  // Skip filling the grown range of the table if the fill value is null, as
  // that is the default value.
  if (!fillValue.isNull() &&
      !tableObj->fillRange(cx, oldLength, delta, fillValue)) {
    return false;
  }
#ifdef DEBUG
  // Assert that null is the default value of the grown range.
  if (fillValue.isNull()) {
    tableObj->assertRangeNull(oldLength, delta);
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
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    JS_FN("type", WasmTableObject::type, 0, JSPROP_ENUMERATE),
#endif
    JS_FN("get", WasmTableObject::get, 1, JSPROP_ENUMERATE),
    JS_FN("set", WasmTableObject::set, 2, JSPROP_ENUMERATE),
    JS_FN("grow", WasmTableObject::grow, 1, JSPROP_ENUMERATE), JS_FS_END};

const JSFunctionSpec WasmTableObject::static_methods[] = {JS_FS_END};

Table& WasmTableObject::table() const {
  return *(Table*)getReservedSlot(TABLE_SLOT).toPrivate();
}

bool WasmTableObject::fillRange(JSContext* cx, uint32_t index, uint32_t length,
                                HandleValue value) const {
  Table& tab = table();

  // All consumers are required to either bounds check or statically be in
  // bounds
  MOZ_ASSERT(uint64_t(index) + uint64_t(length) <= tab.length());

  RootedFunction fun(cx);
  RootedAnyRef any(cx, AnyRef::null());
  if (!CheckRefType(cx, tab.elemType(), value, &fun, &any)) {
    return false;
  }
  switch (tab.repr()) {
    case TableRepr::Func:
      MOZ_RELEASE_ASSERT(!tab.isAsmJS());
      if (!tab.fillFuncRef(Nothing(), index, length,
                           FuncRef::fromJSFunction(fun), cx)) {
        ReportOutOfMemory(cx);
        return false;
      }
      break;
    case TableRepr::Ref:
      tab.fillAnyRef(index, length, any);
      break;
  }
  return true;
}

#ifdef DEBUG
void WasmTableObject::assertRangeNull(uint32_t index, uint32_t length) const {
  Table& tab = table();
  switch (tab.repr()) {
    case TableRepr::Func:
      for (uint32_t i = index; i < index + length; i++) {
        MOZ_ASSERT(tab.getFuncRef(i).tls == nullptr);
        MOZ_ASSERT(tab.getFuncRef(i).code == nullptr);
      }
      break;
    case TableRepr::Ref:
      for (uint32_t i = index; i < index + length; i++) {
        MOZ_ASSERT(tab.getAnyRef(i).isNull());
      }
      break;
  }
}
#endif

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
  global->val().get().trace(trc);
}

/* static */
void WasmGlobalObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmGlobalObject* global = reinterpret_cast<WasmGlobalObject*>(obj);
  if (!global->isNewborn()) {
    fop->delete_(obj, &global->val(), MemoryUse::WasmGlobalCell);
  }
}

/* static */
WasmGlobalObject* WasmGlobalObject::create(JSContext* cx, HandleVal value,
                                           bool isMutable, HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmGlobalObject obj(
      cx, NewObjectWithGivenProto<WasmGlobalObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());
  MOZ_ASSERT(obj->isTenured(), "assumed by global.set post barriers");

  GCPtrVal* val = js_new<GCPtrVal>(Val(value.get().type()));
  if (!val) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
  obj->initReservedSlot(MUTABLE_SLOT, JS::BooleanValue(isMutable));
  InitReservedSlot(obj, VAL_SLOT, val, MemoryUse::WasmGlobalCell);

  // It's simpler to initialize the cell after the object has been created,
  // to avoid needing to root the cell before the object creation.
  obj->val() = value.get();

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

  ValType globalType;
  if (!ToValType(cx, typeVal, &globalType)) {
    return false;
  }

  bool isMutable = ToBoolean(mutableVal);

  // Extract the initial value, or provide a suitable default.
  RootedVal globalVal(cx, globalType);

  // Override with non-undefined value, if provided.
  RootedValue valueVal(cx, args.get(1));
  if (!valueVal.isUndefined() ||
      (args.length() >= 2 && globalType.isRefType())) {
    if (!Val::fromJSValue(cx, globalType, valueVal, &globalVal)) {
      return false;
    }
  }

  RootedObject proto(cx,
                     GetWasmConstructorPrototype(cx, args, JSProto_WasmGlobal));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }

  WasmGlobalObject* global =
      WasmGlobalObject::create(cx, globalVal, isMutable, proto);
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
  const WasmGlobalObject& globalObj =
      args.thisv().toObject().as<WasmGlobalObject>();
  if (!globalObj.type().isExposable()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_VAL_TYPE);
    return false;
  }
  return globalObj.val().get().toJSValue(cx, args.rval());
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

  RootedVal val(cx);
  if (!Val::fromJSValue(cx, global->type(), args.get(0), &val)) {
    return false;
  }
  global->val() = val.get();

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
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    JS_FN("type", WasmGlobalObject::type, 0, JSPROP_ENUMERATE),
#endif
    JS_FN(js_valueOf_str, WasmGlobalObject::valueGetter, 0, JSPROP_ENUMERATE),
    JS_FS_END};

const JSFunctionSpec WasmGlobalObject::static_methods[] = {JS_FS_END};

bool WasmGlobalObject::isMutable() const {
  return getReservedSlot(MUTABLE_SLOT).toBoolean();
}

ValType WasmGlobalObject::type() const { return val().get().type(); }

GCPtrVal& WasmGlobalObject::val() const {
  return *reinterpret_cast<GCPtrVal*>(getReservedSlot(VAL_SLOT).toPrivate());
}

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
/* static */
bool WasmGlobalObject::typeImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmGlobalObject global(
      cx, &args.thisv().toObject().as<WasmGlobalObject>());
  RootedObject typeObj(
      cx, GlobalTypeToObject(cx, global->type(), global->isMutable()));
  if (!typeObj) {
    return false;
  }
  args.rval().setObject(*typeObj);
  return true;
}

/* static */
bool WasmGlobalObject::type(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsGlobal, typeImpl>(cx, args);
}
#endif

// ============================================================================
// WebAssembly.Tag class and methods

const JSClassOps WasmTagObject::classOps_ = {
    nullptr,                  // addProperty
    nullptr,                  // delProperty
    nullptr,                  // enumerate
    nullptr,                  // newEnumerate
    nullptr,                  // resolve
    nullptr,                  // mayResolve
    WasmTagObject::finalize,  // finalize
    nullptr,                  // call
    nullptr,                  // hasInstance
    nullptr,                  // construct
    nullptr,                  // trace
};

const JSClass WasmTagObject::class_ = {
    "WebAssembly.Tag",
    JSCLASS_HAS_RESERVED_SLOTS(WasmTagObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmTagObject::classOps_, &WasmTagObject::classSpec_};

const JSClass& WasmTagObject::protoClass_ = PlainObject::class_;

static constexpr char WasmTagName[] = "Tag";

const ClassSpec WasmTagObject::classSpec_ = {
    CreateWasmConstructor<WasmTagObject, WasmTagName>,
    GenericCreatePrototype<WasmTagObject>,
    WasmTagObject::static_methods,
    nullptr,
    WasmTagObject::methods,
    WasmTagObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

/* static */
void WasmTagObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmTagObject& exnObj = obj->as<WasmTagObject>();
  if (!exnObj.isNewborn()) {
    fop->release(obj, &exnObj.tag(), MemoryUse::WasmTagTag);
    fop->delete_(obj, &exnObj.tagType(), MemoryUse::WasmTagType);
  }
}

static bool IsTag(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmTagObject>();
}

bool WasmTagObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Tag")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Tag", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "tag");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  RootedValue paramsVal(cx);
  if (!JS_GetProperty(cx, obj, "parameters", &paramsVal)) {
    return false;
  }

  ValTypeVector params;
  if (!ParseValTypes(cx, paramsVal, params)) {
    return false;
  }
  TagOffsetVector offsets;
  if (!offsets.resize(params.length())) {
    ReportOutOfMemory(cx);
    return false;
  }
  TagType tagType = TagType(std::move(params), std::move(offsets));
  if (!tagType.computeLayout()) {
    return false;
  }

  RootedObject proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_WasmTag, &proto)) {
    return false;
  }
  if (!proto) {
    proto = GlobalObject::getOrCreatePrototype(cx, JSProto_WasmTag);
  }

  RootedWasmTagObject tagObj(cx, WasmTagObject::create(cx, tagType, proto));
  if (!tagObj) {
    return false;
  }

  args.rval().setObject(*tagObj);
  return true;
}

/* static */
WasmTagObject* WasmTagObject::create(JSContext* cx,
                                     const wasm::TagType& tagType,
                                     HandleObject proto) {
  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmTagObject obj(cx,
                          NewObjectWithGivenProto<WasmTagObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());

  SharedExceptionTag tag = SharedExceptionTag(cx->new_<ExceptionTag>());
  if (!tag) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  InitReservedSlot(obj, TAG_SLOT, tag.forget().take(), MemoryUse::WasmTagTag);

  TagType* newType = js_new<TagType>();
  if (!newType || !newType->clone(tagType)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
  InitReservedSlot(obj, TYPE_SLOT, newType, MemoryUse::WasmTagType);

  MOZ_ASSERT(!obj->isNewborn());

  return obj;
}

bool WasmTagObject::isNewborn() const {
  MOZ_ASSERT(is<WasmTagObject>());
  return getReservedSlot(TYPE_SLOT).isUndefined();
}

const JSPropertySpec WasmTagObject::properties[] = {
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Tag", JSPROP_READONLY),
    JS_PS_END};

#ifdef ENABLE_WASM_TYPE_REFLECTIONS
/* static */
bool WasmTagObject::typeImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmTagObject tag(cx, &args.thisv().toObject().as<WasmTagObject>());
  RootedObject typeObj(cx, TagTypeToObject(cx, tag->valueTypes()));
  if (!typeObj) {
    return false;
  }
  args.rval().setObject(*typeObj);
  return true;
}

/* static  */
bool WasmTagObject::type(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsTag, typeImpl>(cx, args);
}
#endif

const JSFunctionSpec WasmTagObject::methods[] = {
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
    JS_FN("type", WasmTagObject::type, 0, JSPROP_ENUMERATE),
#endif
    JS_FS_END};

const JSFunctionSpec WasmTagObject::static_methods[] = {JS_FS_END};

TagType& WasmTagObject::tagType() const {
  return *(TagType*)getFixedSlot(TYPE_SLOT).toPrivate();
};

wasm::ValTypeVector& WasmTagObject::valueTypes() const {
  return tagType().argTypes;
};

wasm::ResultType WasmTagObject::resultType() const {
  return wasm::ResultType::Vector(valueTypes());
}

ExceptionTag& WasmTagObject::tag() const {
  return *(ExceptionTag*)getReservedSlot(TAG_SLOT).toPrivate();
}

// ============================================================================
// WebAssembly.Exception class and methods

const JSClassOps WasmExceptionObject::classOps_ = {
    nullptr,                        // addProperty
    nullptr,                        // delProperty
    nullptr,                        // enumerate
    nullptr,                        // newEnumerate
    nullptr,                        // resolve
    nullptr,                        // mayResolve
    WasmExceptionObject::finalize,  // finalize
    nullptr,                        // call
    nullptr,                        // hasInstance
    nullptr,                        // construct
    nullptr,                        // trace
};

const JSClass WasmExceptionObject::class_ = {
    "WebAssembly.Exception",
    JSCLASS_HAS_RESERVED_SLOTS(WasmExceptionObject::RESERVED_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &WasmExceptionObject::classOps_, &WasmExceptionObject::classSpec_};

const JSClass& WasmExceptionObject::protoClass_ = PlainObject::class_;

static constexpr char WasmExceptionName[] = "Exception";

const ClassSpec WasmExceptionObject::classSpec_ = {
    CreateWasmConstructor<WasmExceptionObject, WasmExceptionName>,
    GenericCreatePrototype<WasmExceptionObject>,
    WasmExceptionObject::static_methods,
    nullptr,
    WasmExceptionObject::methods,
    WasmExceptionObject::properties,
    nullptr,
    ClassSpec::DontDefineConstructor};

/* static */
void WasmExceptionObject::finalize(JSFreeOp* fop, JSObject* obj) {
  WasmExceptionObject& exnObj = obj->as<WasmExceptionObject>();
  if (!exnObj.isNewborn()) {
    fop->release(obj, &exnObj.tag(), MemoryUse::WasmExceptionTag);
  }
}

static bool IsException(HandleValue v) {
  return v.isObject() && v.toObject().is<WasmExceptionObject>();
}

bool WasmExceptionObject::construct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "Exception")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Exception", 2)) {
    return false;
  }

  if (!IsTag(args[0])) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_EXN_ARG);
    return false;
  }

  RootedWasmTagObject exnTag(cx, &args[0].toObject().as<WasmTagObject>());

  if (!args.get(1).isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_EXN_PAYLOAD);
    return false;
  }

  JS::ForOfIterator iterator(cx);
  if (!iterator.init(args.get(1), JS::ForOfIterator::ThrowOnNonIterable)) {
    return false;
  }

  const wasm::TagType& tagType = exnTag->tagType();
  const wasm::ValTypeVector& params = tagType.argTypes;
  const wasm::TagOffsetVector& offsets = tagType.argOffsets;

  RootedArrayBufferObject buf(
      cx, ArrayBufferObject::createZeroed(cx, tagType.bufferSize));
  if (!buf) {
    return false;
  }

  RootedArrayObject refs(cx, NewDenseFullyAllocatedArray(cx, tagType.refCount));
  if (!refs) {
    return false;
  }
  refs->setDenseInitializedLength(tagType.refCount);
  for (int i = 0; i < tagType.refCount; i++) {
    refs->initDenseElement(i, UndefinedValue());
  }

  uint8_t* bufPtr = buf->dataPointer();
  RootedValue nextArg(cx);
  for (size_t i = 0; i < params.length(); i++) {
    bool done;
    if (!iterator.next(&nextArg, &done)) {
      return false;
    }
    if (done) {
      UniqueChars expected(JS_smprintf("%zu", params.length()));
      UniqueChars got(JS_smprintf("%zu", i));

      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                               JSMSG_WASM_BAD_EXN_PAYLOAD_LEN, expected.get(),
                               got.get());
      return false;
    }

    if (params[i].isRefRepr()) {
      ASSERT_ANYREF_IS_JSOBJECT;
      RootedAnyRef anyref(cx, AnyRef::null());
      if (!ToWebAssemblyValue(cx, nextArg, params[i],
                              anyref.get().asJSObjectAddress(), true)) {
        return false;
      }
      refs->setDenseElement(offsets[i] / sizeof(Value),
                            ObjectValue(*anyref.get().asJSObject()));
    } else {
      if (!ToWebAssemblyValue(cx, nextArg, params[i], bufPtr + offsets[i],
                              true)) {
        return false;
      }
    }
  }

  RootedWasmExceptionObject exnObj(
      cx, WasmExceptionObject::create(cx, SharedExceptionTag(&exnTag->tag()),
                                      buf, refs));

  args.rval().setObject(*exnObj);
  return true;
}

/* static */
WasmExceptionObject* WasmExceptionObject::create(JSContext* cx,
                                                 wasm::SharedExceptionTag tag,
                                                 HandleArrayBufferObject values,
                                                 HandleArrayObject refs) {
  RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmException));

  AutoSetNewObjectMetadata metadata(cx);
  RootedWasmExceptionObject obj(
      cx, NewObjectWithGivenProto<WasmExceptionObject>(cx, proto));
  if (!obj) {
    return nullptr;
  }

  MOZ_ASSERT(obj->isNewborn());
  InitReservedSlot(obj, TAG_SLOT, tag.forget().take(),
                   MemoryUse::WasmExceptionTag);

  obj->initFixedSlot(VALUES_SLOT, ObjectValue(*values));
  obj->initFixedSlot(REFS_SLOT, ObjectValue(*refs));

  MOZ_ASSERT(!obj->isNewborn());

  return obj;
}

bool WasmExceptionObject::isNewborn() const {
  MOZ_ASSERT(is<WasmExceptionObject>());
  return getReservedSlot(REFS_SLOT).isUndefined();
}

const JSPropertySpec WasmExceptionObject::properties[] = {
    JS_STRING_SYM_PS(toStringTag, "WebAssembly.Exception", JSPROP_READONLY),
    JS_PS_END};

/* static */
bool WasmExceptionObject::isImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmExceptionObject exnObj(
      cx, &args.thisv().toObject().as<WasmExceptionObject>());

  if (!args.requireAtLeast(cx, "WebAssembly.Exception.is", 1)) {
    return false;
  }

  if (!IsTag(args[0])) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_EXN_ARG);
    return false;
  }

  RootedWasmTagObject exnTag(cx, &args.get(0).toObject().as<WasmTagObject>());
  args.rval().setBoolean(&exnTag->tag() == &exnObj->tag());

  return true;
}

/* static  */
bool WasmExceptionObject::isMethod(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsException, isImpl>(cx, args);
}

/* static */
bool WasmExceptionObject::getArgImpl(JSContext* cx, const CallArgs& args) {
  RootedWasmExceptionObject exnObj(
      cx, &args.thisv().toObject().as<WasmExceptionObject>());

  if (!args.requireAtLeast(cx, "WebAssembly.Exception.getArg", 2)) {
    return false;
  }

  if (!IsTag(args[0])) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_EXN_ARG);
    return false;
  }

  RootedWasmTagObject exnTag(cx, &args.get(0).toObject().as<WasmTagObject>());
  if (&exnTag->tag() != &exnObj->tag()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_EXN_TAG);
    return false;
  }

  uint32_t index;
  if (!EnforceRangeU32(cx, args.get(1), "Exception", "getArg index", &index)) {
    return false;
  }

  wasm::ValTypeVector& params = exnTag->valueTypes();
  if (index >= params.length()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_RANGE,
                             "Exception", "getArg index");
    return false;
  }

  uint32_t offset = exnTag->tagType().argOffsets[index];
  RootedValue result(cx);
  if (params[index].isRefRepr()) {
    ASSERT_ANYREF_IS_JSOBJECT;
    RootedValue val(cx, exnObj->refs().getDenseElement(offset / sizeof(Value)));
    RootedAnyRef anyref(cx, AnyRef::fromJSObject(val.toObjectOrNull()));
    if (!ToJSValue(cx, anyref.get().asJSObjectAddress(), params[index],
                   &result)) {
      return false;
    }
  } else {
    if (!ToJSValue(cx, exnObj->values().dataPointer() + offset, params[index],
                   &result)) {
      return false;
    }
  }

  args.rval().set(result);
  return true;
}

/* static  */
bool WasmExceptionObject::getArg(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsException, getArgImpl>(cx, args);
}

const JSFunctionSpec WasmExceptionObject::methods[] = {
    JS_FN("is", WasmExceptionObject::isMethod, 1, JSPROP_ENUMERATE),
    JS_FN("getArg", WasmExceptionObject::getArg, 2, JSPROP_ENUMERATE),
    JS_FS_END};

const JSFunctionSpec WasmExceptionObject::static_methods[] = {JS_FS_END};

ExceptionTag& WasmExceptionObject::tag() const {
  return *(ExceptionTag*)getReservedSlot(TAG_SLOT).toPrivate();
}

ArrayBufferObject& WasmExceptionObject::values() const {
  return getReservedSlot(VALUES_SLOT).toObject().as<ArrayBufferObject>();
}

ArrayObject& WasmExceptionObject::refs() const {
  return getReservedSlot(REFS_SLOT).toObject().as<ArrayObject>();
}

// ============================================================================
// WebAssembly.Function and methods
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
static JSObject* CreateWasmFunctionPrototype(JSContext* cx, JSProtoKey key) {
  // WasmFunction's prototype should inherit from JSFunction's prototype.
  RootedObject jsProto(
      cx, GlobalObject::getOrCreatePrototype(cx, JSProto_Function));
  if (!jsProto) {
    return nullptr;
  }
  return GlobalObject::createBlankPrototypeInheriting(cx, &PlainObject::class_,
                                                      jsProto);
}

[[nodiscard]] static bool IsWasmFunction(HandleValue v) {
  if (!v.isObject()) {
    return false;
  }
  if (!v.toObject().is<JSFunction>()) {
    return false;
  }
  return v.toObject().as<JSFunction>().isWasm();
}

bool WasmFunctionTypeImpl(JSContext* cx, const CallArgs& args) {
  RootedFunction function(cx, &args.thisv().toObject().as<JSFunction>());
  RootedWasmInstanceObject instanceObj(
      cx, ExportedFunctionToInstanceObject(function));
  uint32_t funcIndex = ExportedFunctionToFuncIndex(function);
  Instance& instance = instanceObj->instance();
  const FuncType& ft = instance.metadata(instance.code().bestTier())
                           .lookupFuncExport(funcIndex)
                           .funcType();
  RootedObject typeObj(cx, FuncTypeToObject(cx, ft));
  if (!typeObj) {
    return false;
  }
  args.rval().setObject(*typeObj);
  return true;
}

bool WasmFunctionType(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return CallNonGenericMethod<IsWasmFunction, WasmFunctionTypeImpl>(cx, args);
}

JSFunction* WasmFunctionCreate(JSContext* cx, HandleFunction func,
                               wasm::ValTypeVector&& params,
                               wasm::ValTypeVector&& results,
                               HandleObject proto) {
  MOZ_RELEASE_ASSERT(!IsWasmExportedFunction(func));

  // We want to import the function to a wasm module and then export it again so
  // that it behaves exactly like a normal wasm function and can be used like
  // one in wasm tables. We synthesize such a module below, instantiate it, and
  // then return the exported function as the result.
  FeatureOptions options;
  ScriptedCaller scriptedCaller;
  SharedCompileArgs compileArgs =
      CompileArgs::build(cx, std::move(scriptedCaller), options);
  if (!compileArgs) {
    return nullptr;
  }

  ModuleEnvironment moduleEnv(compileArgs->features);
  CompilerEnvironment compilerEnv(CompileMode::Once, Tier::Optimized,
                                  OptimizedBackend::Ion, DebugEnabled::False);
  compilerEnv.computeParameters();

  // Initialize the type section
  if (!moduleEnv.initTypes(1)) {
    return nullptr;
  }
  FuncType funcType = FuncType(std::move(params), std::move(results));
  (*moduleEnv.types)[0] = TypeDef(std::move(funcType));

  // Add an (import (func ...))
  FuncDesc funcDesc =
      FuncDesc(&(*moduleEnv.types)[0].funcType(), &moduleEnv.typeIds[0], 0);
  if (!moduleEnv.funcs.append(funcDesc) ||
      !moduleEnv.funcImportGlobalDataOffsets.resize(1)) {
    return nullptr;
  }

  // Add an (export (func 0))
  moduleEnv.declareFuncExported(0, false, false);

  // We will be looking up and using the function in the future by index so the
  // name doesn't matter.
  CacheableChars fieldName = DuplicateString("");
  if (!moduleEnv.exports.emplaceBack(std::move(fieldName), 0,
                                     DefinitionKind::Function)) {
    return nullptr;
  }

  ModuleGenerator mg(*compileArgs, &moduleEnv, &compilerEnv, nullptr, nullptr,
                     nullptr);
  if (!mg.init(nullptr)) {
    return nullptr;
  }
  // We're not compiling any function definitions.
  if (!mg.finishFuncDefs()) {
    return nullptr;
  }
  ShareableBytes* shareableBytes = cx->new_<ShareableBytes>();
  if (!shareableBytes) {
    return nullptr;
  }
  SharedModule module = mg.finishModule(*shareableBytes);
  if (!module) {
    return nullptr;
  }

  // Instantiate the module.
  Rooted<ImportValues> imports(cx);
  if (!imports.get().funcs.append(func)) {
    return nullptr;
  }
  RootedWasmInstanceObject instance(cx);
  if (!module->instantiate(cx, imports.get(), nullptr, &instance)) {
    MOZ_ASSERT(cx->isThrowingOutOfMemory());
    return nullptr;
  }

  // Get the exported function which wraps the JS function to return.
  RootedFunction wasmFunc(cx);
  if (!instance->getExportedFunction(cx, instance, 0, &wasmFunc)) {
    return nullptr;
  }
  return wasmFunc;
}

bool WasmFunctionConstruct(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "WebAssembly.Function")) {
    return false;
  }

  if (!args.requireAtLeast(cx, "WebAssembly.Function", 2)) {
    return false;
  }

  if (!args[0].isObject()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_DESC_ARG, "function");
    return false;
  }
  RootedObject typeObj(cx, &args[0].toObject());

  // Extract properties in lexicographic order per spec.

  RootedValue parametersVal(cx);
  if (!JS_GetProperty(cx, typeObj, "parameters", &parametersVal)) {
    return false;
  }

  ValTypeVector params;
  if (!ParseValTypes(cx, parametersVal, params)) {
    return false;
  }

  RootedValue resultsVal(cx);
  if (!JS_GetProperty(cx, typeObj, "results", &resultsVal)) {
    return false;
  }

  ValTypeVector results;
  if (!ParseValTypes(cx, resultsVal, results)) {
    return false;
  }

  // Get the target function

  if (!args[1].isObject() || !args[1].toObject().is<JSFunction>() ||
      IsWasmFunction(args[1])) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_FUNCTION_VALUE);
    return false;
  }
  RootedFunction func(cx, &args[1].toObject().as<JSFunction>());

  RootedObject proto(
      cx, GetWasmConstructorPrototype(cx, args, JSProto_WasmFunction));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }

  RootedFunction wasmFunc(cx, WasmFunctionCreate(cx, func, std::move(params),
                                                 std::move(results), proto));
  if (!wasmFunc) {
    ReportOutOfMemory(cx);
    return false;
  }
  args.rval().setObject(*wasmFunc);

  return true;
}

static constexpr char WasmFunctionName[] = "Function";

static JSObject* CreateWasmFunctionConstructor(JSContext* cx, JSProtoKey key) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateFunctionConstructor(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  RootedAtom className(cx,
                       Atomize(cx, WasmFunctionName, strlen(WasmFunctionName)));
  if (!className) {
    return nullptr;
  }
  return NewFunctionWithProto(cx, WasmFunctionConstruct, 1,
                              FunctionFlags::NATIVE_CTOR, nullptr, className,
                              proto, gc::AllocKind::FUNCTION, TenuredObject);
}

const JSFunctionSpec WasmFunctionMethods[] = {
    JS_FN("type", WasmFunctionType, 0, 0), JS_FS_END};

const ClassSpec WasmFunctionClassSpec = {CreateWasmFunctionConstructor,
                                         CreateWasmFunctionPrototype,
                                         nullptr,
                                         nullptr,
                                         WasmFunctionMethods,
                                         nullptr,
                                         nullptr,
                                         ClassSpec::DontDefineConstructor};

const JSClass js::WasmFunctionClass = {
    "WebAssembly.Function", 0, JS_NULL_CLASS_OPS, &WasmFunctionClassSpec};

#endif

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

  // There's no error |cause| available here.
  auto cause = JS::NothingHandleValue;

  RootedObject errorObj(
      cx, ErrorObject::create(cx, JSEXN_WASMCOMPILEERROR, stack, filename, 0,
                              line, 0, nullptr, message, cause));
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
        cx, &cx->global()->getPrototype(JSProto_WasmInstance));

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

      RootedObject moduleProto(cx,
                               &cx->global()->getPrototype(JSProto_WasmModule));
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
  RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmModule));
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

  bool init(JSContext* cx, HandleValue maybeOptions, const char* introducer) {
    compileArgs = InitCompileArgs(cx, maybeOptions, introducer);
    if (!compileArgs) {
      return false;
    }
    return PromiseHelperTask::init(cx);
  }

  void execute() override {
    module = CompileBuffer(*compileArgs, *bytecode, &error, &warnings, nullptr);
  }

  bool resolve(JSContext* cx, Handle<PromiseObject*> promise) override {
    if (!ReportCompileWarnings(cx, warnings)) {
      return false;
    }
    if (!module) {
      return Reject(cx, *compileArgs, promise, error);
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

  CallArgs callArgs = CallArgsFromVp(argc, vp);

  auto task = cx->make_unique<CompileBufferTask>(cx, promise);
  if (!task || !task->init(cx, callArgs.get(1), "WebAssembly.compile")) {
    return false;
  }

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
    if (!task || !task->init(cx, callArgs.get(2), "WebAssembly.instantiate")) {
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

  FeatureOptions options;
  ParseCompileOptions(cx, callArgs.get(1), &options);
  UniqueChars error;
  bool validated = Validate(cx, *bytecode, options, &error);

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
        module_ = CompileBuffer(*compileArgs_, *bytecode, &compileError_,
                                &warnings_, nullptr);
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

    if (!ReportCompileWarnings(cx, warnings_)) {
      return false;
    }
    if (module_) {
      MOZ_ASSERT(!streamFailed_ && !streamError_ && !compileError_);
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
    return RejectWithErrorNumber(cx, JSMSG_WASM_BAD_RESPONSE_VALUE, promise);
  }

  RootedObject response(cx, &callArgs.get(0).toObject());
  if (!cx->runtime()->consumeStreamCallback(cx, response, JS::MimeType::Wasm,
                                            task.get())) {
    return RejectWithPendingException(cx, promise);
  }

  (void)task.release();

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

  SharedCompileArgs compileArgs =
      InitCompileArgs(cx, callArgs.get(instantiate ? 2 : 1), introducer);
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

#ifdef ENABLE_WASM_MOZ_INTGEMM

static bool WebAssembly_mozIntGemm(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedWasmModuleObject module(cx);
  if (!wasm::CompileIntrinsicModule(cx, mozilla::Span<IntrinsicOp>(),
                                    Shareable::True, &module)) {
    ReportOutOfMemory(cx);
    return false;
  }
  args.rval().set(ObjectValue(*module.get()));
  return true;
}

static const JSFunctionSpec WebAssembly_mozIntGemm_methods[] = {
    JS_FN("mozIntGemm", WebAssembly_mozIntGemm, 0, JSPROP_ENUMERATE),
    JS_FS_END};

#endif  // ENABLE_WASM_MOZ_INTGEMM

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
  return NewTenuredObjectWithGivenProto(cx, &WasmNamespaceObject::class_,
                                        proto);
}

struct NameAndProtoKey {
  const char* const name;
  JSProtoKey key;
};

static bool WebAssemblyDefineConstructor(JSContext* cx,
                                         Handle<WasmNamespaceObject*> wasm,
                                         NameAndProtoKey entry,
                                         MutableHandleValue ctorValue,
                                         MutableHandleId id) {
  JSObject* ctor = GlobalObject::getOrCreateConstructor(cx, entry.key);
  if (!ctor) {
    return false;
  }
  ctorValue.setObject(*ctor);

  JSAtom* className = Atomize(cx, entry.name, strlen(entry.name));
  if (!className) {
    return false;
  }
  id.set(AtomToId(className));

  if (!DefineDataProperty(cx, wasm, id, ctorValue, 0)) {
    return false;
  }
  return true;
}

static bool WebAssemblyClassFinish(JSContext* cx, HandleObject object,
                                   HandleObject proto) {
  Handle<WasmNamespaceObject*> wasm = object.as<WasmNamespaceObject>();

  constexpr NameAndProtoKey entries[] = {
      {"Module", JSProto_WasmModule},
      {"Instance", JSProto_WasmInstance},
      {"Memory", JSProto_WasmMemory},
      {"Table", JSProto_WasmTable},
      {"Global", JSProto_WasmGlobal},
      {"CompileError", GetExceptionProtoKey(JSEXN_WASMCOMPILEERROR)},
      {"LinkError", GetExceptionProtoKey(JSEXN_WASMLINKERROR)},
      {"RuntimeError", GetExceptionProtoKey(JSEXN_WASMRUNTIMEERROR)},
#ifdef ENABLE_WASM_TYPE_REFLECTIONS
      {"Function", JSProto_WasmFunction},
#endif
  };
  RootedValue ctorValue(cx);
  RootedId id(cx);
  for (const auto& entry : entries) {
    if (!WebAssemblyDefineConstructor(cx, wasm, entry, &ctorValue, &id)) {
      return false;
    }
  }

#ifdef ENABLE_WASM_EXCEPTIONS
  if (ExceptionsAvailable(cx)) {
    constexpr NameAndProtoKey exceptionEntries[] = {
        {"Tag", JSProto_WasmTag},
        {"Exception", JSProto_WasmException},
    };
    for (const auto& entry : exceptionEntries) {
      if (!WebAssemblyDefineConstructor(cx, wasm, entry, &ctorValue, &id)) {
        return false;
      }
    }
  }
#endif

#ifdef ENABLE_WASM_MOZ_INTGEMM
  if (MozIntGemmAvailable(cx) &&
      !JS_DefineFunctions(cx, wasm, WebAssembly_mozIntGemm_methods)) {
    return false;
  }
#endif

  return true;
}

static const ClassSpec WebAssemblyClassSpec = {CreateWebAssemblyObject,
                                               nullptr,
                                               WebAssembly_static_methods,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               WebAssemblyClassFinish};

const JSClass js::WasmNamespaceObject::class_ = {
    js_WebAssembly_str, JSCLASS_HAS_CACHED_PROTO(JSProto_WebAssembly),
    JS_NULL_CLASS_OPS, &WebAssemblyClassSpec};

// Sundry
