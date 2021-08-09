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

bool EncodeIntrinsicBody(const Intrinsic& intrinsic, IntrinsicOp op,
                         Bytes* body) {
  Encoder encoder(*body);
  if (!EncodeLocalEntries(encoder, ValTypeVector())) {
    return false;
  }
  for (uint32_t i = 0; i < intrinsic.params.size(); i++) {
    if (!encoder.writeOp(Op::GetLocal) || !encoder.writeVarU32(i)) {
      return false;
    }
  }
  if (!encoder.writeOp(op)) {
    return false;
  }
  if (!encoder.writeOp(Op::End)) {
    return false;
  }
  return true;
}

bool wasm::CompileIntrinsicModule(JSContext* cx,
                                  const mozilla::Span<IntrinsicOp> ops,
                                  Shareable sharedMemory,
                                  MutableHandleWasmModuleObject result) {
  // Create the options manually, enabling intrinsics
  FeatureOptions featureOptions;
  featureOptions.intrinsics = true;

  // Initialize the compiler environment, choosing the best tier possible
  SharedCompileArgs compileArgs =
      CompileArgs::build(cx, ScriptedCaller(), featureOptions);
  if (!compileArgs) {
    return false;
  }
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
    ReportOutOfMemory(cx);
    return false;
  }
  moduleEnv.memory = Some(MemoryDesc(Limits(0, Nothing(), sharedMemory)));

  // Add (type (func (params ...))) for each intrinsic. The function types will
  // be deduplicated by the runtime
  for (uint32_t funcIndex = 0; funcIndex < ops.size(); funcIndex++) {
    const Intrinsic& intrinsic = Intrinsic::getFromOp(ops[funcIndex]);

    FuncType type;
    if (!intrinsic.funcType(&type) ||
        !moduleEnv.types.append(TypeDef(std::move(type))) ||
        !moduleEnv.typeIds.append(TypeIdDesc())) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  // Add (func (type $i)) declarations. Do this after all types have been added
  // as the function declaration metadata uses pointers into the type vectors
  // that must be stable.
  for (uint32_t funcIndex = 0; funcIndex < ops.size(); funcIndex++) {
    FuncDesc decl(&moduleEnv.types[funcIndex].funcType(),
                  &moduleEnv.typeIds[funcIndex], funcIndex);
    if (!moduleEnv.funcs.append(decl)) {
      ReportOutOfMemory(cx);
      return false;
    }
    moduleEnv.declareFuncExported(funcIndex, true, false);
  }

  // Add (export "$name" (func $i)) declarations.
  for (uint32_t funcIndex = 0; funcIndex < ops.size(); funcIndex++) {
    const Intrinsic& intrinsic = Intrinsic::getFromOp(ops[funcIndex]);

    UniqueChars exportString = DuplicateString(intrinsic.exportName);
    if (!exportString ||
        !moduleEnv.exports.append(Export(std::move(exportString), funcIndex,
                                         DefinitionKind::Function))) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  // Compile the module functions
  UniqueChars error;
  ModuleGenerator mg(*compileArgs, &moduleEnv, &compilerEnv, nullptr, &error);
  if (!mg.init(nullptr)) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Prepare and compile function bodies
  Vector<Bytes, 1, SystemAllocPolicy> bodies;
  if (!bodies.reserve(ops.size())) {
    ReportOutOfMemory(cx);
    return false;
  }
  for (uint32_t funcIndex = 0; funcIndex < ops.size(); funcIndex++) {
    IntrinsicOp op = ops[funcIndex];
    const Intrinsic& intrinsic = Intrinsic::getFromOp(ops[funcIndex]);

    // Compilation may be done using other threads, ModuleGenerator requires
    // that function bodies live until after finishFuncDefs().
    bodies.infallibleAppend(Bytes());
    Bytes& bytecode = bodies.back();

    // Encode function body that will call the intrinsic using our builtin
    // opcode, and launch a compile task
    if (!EncodeIntrinsicBody(intrinsic, op, &bytecode) ||
        !mg.compileFuncDef(funcIndex, 0, bytecode.begin(),
                           bytecode.begin() + bytecode.length())) {
      // This must be an OOM and will be reported by the caller
      MOZ_ASSERT(!error);
      ReportOutOfMemory(cx);
      return false;
    }
  }

  // Finish and block on function compilation
  if (!mg.finishFuncDefs()) {
    // This must be an OOM and will be reported by the caller
    MOZ_ASSERT(!error);
    ReportOutOfMemory(cx);
    return false;
  }

  // Finish the module
  SharedBytes bytecode = js_new<ShareableBytes>();
  SharedModule module = mg.finishModule(*bytecode, nullptr);
  if (!module) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Create a WasmModuleObject for the module, and return it
  RootedObject proto(
      cx, GlobalObject::getOrCreatePrototype(cx, JSProto_WasmModule));
  if (!proto) {
    ReportOutOfMemory(cx);
    return false;
  }
  result.set(WasmModuleObject::create(cx, *module, proto));
  return !!result;
}
