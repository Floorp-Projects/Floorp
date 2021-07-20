/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2021 Mozilla Foundation
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

#include "wasm/WasmIntrinsic.h"

#include "util/Text.h"
#include "vm/GlobalObject.h"

#include "wasm/WasmGenerator.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmOpIter.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::wasm;

static const ValType IntrinsicI8VecMul_Params[] = {ValType::I32, ValType::I32,
                                                   ValType::I32, ValType::I32};
const Intrinsic IntrinsicI8VecMul = {
    "i8vecmul",
    mozilla::Span<const ValType>(IntrinsicI8VecMul_Params),
    SASigIntrI8VecMul,
};

bool Intrinsic::funcType(FuncType* type) const {
  ValTypeVector paramVec;
  if (!paramVec.append(params.data(), params.data() + params.size())) {
    return false;
  }
  *type = FuncType(std::move(paramVec), ValTypeVector());
  return true;
}

/* static */
const Intrinsic& Intrinsic::getFromOp(IntrinsicOp op) {
  switch (op) {
    case IntrinsicOp::I8VecMul:
      return IntrinsicI8VecMul;
    default:
      MOZ_CRASH("unexpected intrinsic");
  }
}

bool EncodeIntrinsicBody(const Intrinsic& intrinsic, Bytes* body) {
  Encoder encoder(*body);
  if (!EncodeLocalEntries(encoder, ValTypeVector())) {
    return false;
  }
  for (uint32_t i = 0; i < intrinsic.params.size(); i++) {
    if (!encoder.writeOp(Op::GetLocal) || !encoder.writeVarU32(i)) {
      return false;
    }
  }
  if (!encoder.writeOp(IntrinsicOp::I8VecMul)) {
    return false;
  }
  if (!encoder.writeOp(Op::End)) {
    return false;
  }
  return true;
}

bool wasm::CompileIntrinsicModule(JSContext* cx, IntrinsicOp op,
                                  MutableHandleWasmModuleObject result) {
  const Intrinsic& intrinsic = Intrinsic::getFromOp(op);

  // Create the options manually, enabling intrinsics
  FeatureOptions featureOptions;
  featureOptions.intrinsics = true;

  // Initialize the compiler environment, choosing the best tier possible
  SharedCompileArgs compileArgs =
      CompileArgs::build(cx, ScriptedCaller(), featureOptions);
  CompilerEnvironment compilerEnv(
      CompileMode::Once, IonAvailable(cx) ? Tier::Optimized : Tier::Baseline,
      OptimizedBackend::Ion, DebugEnabled::False);
  compilerEnv.computeParameters();

  // Build a module environment
  ModuleEnvironment moduleEnv(compileArgs->features);

  // Add (import (memory 0))
  UniqueChars emptyString = DuplicateString("");
  UniqueChars memoryString = DuplicateString("memory");
  if (!emptyString || !memoryString ||
      !moduleEnv.imports.append(Import(std::move(emptyString),
                                       std::move(memoryString),
                                       DefinitionKind::Memory))) {
    return false;
  }
  moduleEnv.memory = Some(MemoryDesc(Limits(0)));

  // Add (type (func (params ...))) for the intrinsic
  FuncType type;
  if (!intrinsic.funcType(&type) ||
      !moduleEnv.types.append(TypeDef(std::move(type))) ||
      !moduleEnv.typeIds.append(TypeIdDesc())) {
    return false;
  }

  // Add (func (type 0)) declaration
  FuncDesc decl(&moduleEnv.types[0].funcType(), &moduleEnv.typeIds[0], 0);
  if (!moduleEnv.funcs.append(decl)) {
    return false;
  }
  moduleEnv.declareFuncExported(0, true, false);

  // Encode func body that will call the intrinsic using our builtin opcode
  Bytes intrinsicBody;
  if (!EncodeIntrinsicBody(intrinsic, &intrinsicBody)) {
    return false;
  }

  // Add (export "$name" (func 0))
  UniqueChars exportString = DuplicateString(intrinsic.exportName);
  if (!exportString ||
      !moduleEnv.exports.append(
          Export(std::move(exportString), 0, DefinitionKind::Function))) {
    return false;
  }

  // Compile the module functions
  UniqueChars error;
  ModuleGenerator mg(*compileArgs, &moduleEnv, &compilerEnv, nullptr, &error);
  if (!mg.init(nullptr) ||
      !mg.compileFuncDef(0, 0, intrinsicBody.begin(),
                         intrinsicBody.begin() + intrinsicBody.length()) ||
      !mg.finishFuncDefs()) {
    // This must be an OOM and will be reported by the caller
    MOZ_ASSERT(!error);
    return false;
  }

  // Finish the module
  SharedBytes bytecode = js_new<ShareableBytes>();
  SharedModule module = mg.finishModule(*bytecode, nullptr);
  if (!module) {
    return false;
  }

  // Create a WasmModuleObject for the module, and return it
  RootedObject proto(
      cx, GlobalObject::getOrCreatePrototype(cx, JSProto_WasmModule));
  if (!proto) {
    return false;
  }
  result.set(WasmModuleObject::create(cx, *module, proto));
  return !!result;
}
