/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/MacroAssembler.h"

#include "jsapi-tests/tests.h"
#include "jsapi-tests/testsJit.h"

#include "wasm/WasmConstants.h"
#include "wasm/WasmFeatures.h"  // AnyCompilerAvailable
#include "wasm/WasmGenerator.h"
#include "wasm/WasmSignalHandlers.h"  // EnsureFullSignalHandlers
#include "wasm/WasmValType.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

static bool TestTruncFn(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  double d = args[0].toDouble();
  args.rval().setInt32((int)d);
  return true;
}

// Check if wasm modules can be encoded in C++ and run.
BEGIN_TEST(testWasmEncodeBasic) {
  if (!AnyCompilerAvailable(cx)) {
    knownFail = true;
    return false;
  }

  EnsureFullSignalHandlers(cx);

  FeatureOptions options;
  ScriptedCaller scriptedCaller;
  SharedCompileArgs compileArgs =
      CompileArgs::buildAndReport(cx, std::move(scriptedCaller), options);

  ModuleEnvironment moduleEnv(compileArgs->features);
  CompilerEnvironment compilerEnv(CompileMode::Once, Tier::Optimized,
                                  DebugEnabled::False);
  compilerEnv.computeParameters();
  MOZ_ALWAYS_TRUE(moduleEnv.init());

  ValTypeVector paramsImp, resultsImp;
  MOZ_ALWAYS_TRUE(paramsImp.emplaceBack(ValType::F64) &&
                  resultsImp.emplaceBack(ValType::I32));

  CacheableName ns;
  CacheableName impName;
  MOZ_ALWAYS_TRUE(CacheableName::fromUTF8Chars("t", &impName));
  MOZ_ALWAYS_TRUE(moduleEnv.addImportedFunc(std::move(paramsImp),
                                            std::move(resultsImp),
                                            std::move(ns), std::move(impName)));

  ValTypeVector params, results;
  MOZ_ALWAYS_TRUE(results.emplaceBack(ValType::I32));
  CacheableName expName;
  MOZ_ALWAYS_TRUE(CacheableName::fromUTF8Chars("r", &expName));
  MOZ_ALWAYS_TRUE(moduleEnv.addDefinedFunc(std::move(params),
                                           std::move(results), true,
                                           mozilla::Some(std::move(expName))));

  ModuleGenerator mg(*compileArgs, &moduleEnv, &compilerEnv, nullptr, nullptr,
                     nullptr);
  MOZ_ALWAYS_TRUE(mg.init(nullptr));

  // Build function and keep bytecode around until the end.
  Bytes bytecode;
  {
    Encoder encoder(bytecode);
    MOZ_ALWAYS_TRUE(EncodeLocalEntries(encoder, ValTypeVector()));
    MOZ_ALWAYS_TRUE(encoder.writeOp(Op::F64Const) &&
                    encoder.writeFixedF64(42.3));
    MOZ_ALWAYS_TRUE(encoder.writeOp(Op::Call) && encoder.writeVarU32(0));
    MOZ_ALWAYS_TRUE(encoder.writeOp(Op::End));
  }
  MOZ_ALWAYS_TRUE(mg.compileFuncDef(1, 0, bytecode.begin(),
                                    bytecode.begin() + bytecode.length()));
  MOZ_ALWAYS_TRUE(mg.finishFuncDefs());

  SharedBytes shareableBytes = js_new<ShareableBytes>();
  MOZ_ALWAYS_TRUE(shareableBytes);
  SharedModule module = mg.finishModule(*shareableBytes);
  MOZ_ALWAYS_TRUE(module);

  MOZ_ASSERT(module->imports().length() == 1);
  MOZ_ASSERT(module->exports().length() == 1);

  // Instantiate and run.
  {
    Rooted<ImportValues> imports(cx);
    RootedFunction func(cx, NewNativeFunction(cx, TestTruncFn, 0, nullptr));
    MOZ_ALWAYS_TRUE(func);
    MOZ_ALWAYS_TRUE(imports.get().funcs.append(func));

    Rooted<WasmInstanceObject*> instance(cx);
    MOZ_ALWAYS_TRUE(module->instantiate(cx, imports.get(), nullptr, &instance));
    RootedFunction wasmFunc(cx);
    MOZ_ALWAYS_TRUE(
        WasmInstanceObject::getExportedFunction(cx, instance, 1, &wasmFunc));

    JSAutoRealm ar(cx, wasmFunc);
    RootedValue rval(cx);
    RootedValue fval(cx);
    MOZ_ALWAYS_TRUE(
        JS::Call(cx, fval, wasmFunc, JS::HandleValueArray::empty(), &rval));
    MOZ_RELEASE_ASSERT(rval.toInt32() == 42);
  }
  return true;
}
END_TEST(testWasmEncodeBasic)
