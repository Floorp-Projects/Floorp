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

#include "wasm/WasmBuiltinModule.h"

#include "util/Text.h"
#include "vm/GlobalObject.h"

#include "wasm/WasmBuiltinModuleGenerated.h"
#include "wasm/WasmFeatures.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmOpIter.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::wasm;

#define VISIT_BUILTIN_FUNC(op, export, sa_name, abitype, entry, uses_memory, \
                           idx)                                              \
  static const ValType BuiltinModuleFunc##op##_Params[] =                    \
      DECLARE_BUILTIN_MODULE_FUNC_PARAM_VALTYPES_##op;                       \
                                                                             \
  const BuiltinModuleFunc BuiltinModuleFunc##op = {                          \
      export,                                                                \
      mozilla::Span<const ValType>(BuiltinModuleFunc##op##_Params),          \
      DECLARE_BUILTIN_MODULE_FUNC_RESULT_VALTYPE_##op,                       \
      SASig##sa_name,                                                        \
      uses_memory,                                                           \
  };

FOR_EACH_BUILTIN_MODULE_FUNC(VISIT_BUILTIN_FUNC)
#undef VISIT_BUILTIN_FUNC

bool BuiltinModuleFunc::funcType(FuncType* type) const {
  ValTypeVector paramVec;
  if (!paramVec.append(params.data(), params.data() + params.size())) {
    return false;
  }
  ValTypeVector resultVec;
  if (result.isSome() && !resultVec.append(*result)) {
    return false;
  }
  *type = FuncType(std::move(paramVec), std::move(resultVec));
  return true;
}

/* static */
const BuiltinModuleFunc& BuiltinModuleFunc::getFromId(BuiltinModuleFuncId id) {
  switch (id) {
#define VISIT_BUILTIN_FUNC(op, ...) \
  case BuiltinModuleFuncId::op:     \
    return BuiltinModuleFunc##op;
    FOR_EACH_BUILTIN_MODULE_FUNC(VISIT_BUILTIN_FUNC)
#undef VISIT_BUILTIN_FUNC
    default:
      MOZ_CRASH("unexpected builtinModuleFunc");
  }
}

bool EncodeFuncBody(const BuiltinModuleFunc& builtinModuleFunc,
                    BuiltinModuleFuncId id, Bytes* body) {
  Encoder encoder(*body);
  if (!EncodeLocalEntries(encoder, ValTypeVector())) {
    return false;
  }
  for (uint32_t i = 0; i < builtinModuleFunc.params.size(); i++) {
    if (!encoder.writeOp(Op::LocalGet) || !encoder.writeVarU32(i)) {
      return false;
    }
  }
  if (!encoder.writeOp(MozOp::CallBuiltinModuleFunc)) {
    return false;
  }
  if (!encoder.writeVarU32(uint32_t(id))) {
    return false;
  }
  return encoder.writeOp(Op::End);
}

bool wasm::CompileBuiltinModule(JSContext* cx,
                                const mozilla::Span<BuiltinModuleFuncId> ids,
                                mozilla::Maybe<Shareable> memory,
                                MutableHandle<WasmModuleObject*> result) {
  // Create the options manually, enabling intrinsics
  FeatureOptions featureOptions;
  featureOptions.isBuiltinModule = true;

  // Initialize the compiler environment, choosing the best tier possible
  SharedCompileArgs compileArgs = CompileArgs::buildAndReport(
      cx, ScriptedCaller(), featureOptions, /* reportOOM */ true);
  if (!compileArgs) {
    return false;
  }
  CompilerEnvironment compilerEnv(
      CompileMode::Once, IonAvailable(cx) ? Tier::Optimized : Tier::Baseline,
      DebugEnabled::False);
  compilerEnv.computeParameters();

  // Build a module environment
  ModuleEnvironment moduleEnv(compileArgs->features);
  if (!moduleEnv.init()) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (memory.isSome()) {
    // Add (import (memory 0))
    CacheableName emptyString;
    CacheableName memoryString;
    if (!CacheableName::fromUTF8Chars("memory", &memoryString)) {
      ReportOutOfMemory(cx);
      return false;
    }
    if (!moduleEnv.imports.append(Import(std::move(emptyString),
                                         std::move(memoryString),
                                         DefinitionKind::Memory))) {
      ReportOutOfMemory(cx);
      return false;
    }
    if (!moduleEnv.memories.append(MemoryDesc(Limits(0, Nothing(), *memory)))) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  // Add (type (func (params ...))) for each func. The function types will
  // be deduplicated by the runtime
  for (uint32_t funcIndex = 0; funcIndex < ids.size(); funcIndex++) {
    const BuiltinModuleFuncId& id = ids[funcIndex];
    const BuiltinModuleFunc& builtinModuleFunc =
        BuiltinModuleFunc::getFromId(id);

    FuncType type;
    if (!builtinModuleFunc.funcType(&type) ||
        !moduleEnv.types->addType(std::move(type))) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  // Add (func (type $i)) declarations. Do this after all types have been added
  // as the function declaration metadata uses pointers into the type vectors
  // that must be stable.
  for (uint32_t funcIndex = 0; funcIndex < ids.size(); funcIndex++) {
    FuncDesc decl(&(*moduleEnv.types)[funcIndex].funcType(), funcIndex);
    if (!moduleEnv.funcs.append(decl)) {
      ReportOutOfMemory(cx);
      return false;
    }
    moduleEnv.declareFuncExported(funcIndex, true, false);
  }

  // Add (export "$name" (func $i)) declarations.
  for (uint32_t funcIndex = 0; funcIndex < ids.size(); funcIndex++) {
    const BuiltinModuleFunc& builtinModuleFunc =
        BuiltinModuleFunc::getFromId(ids[funcIndex]);

    CacheableName exportName;
    if (!CacheableName::fromUTF8Chars(builtinModuleFunc.exportName,
                                      &exportName) ||
        !moduleEnv.exports.append(Export(std::move(exportName), funcIndex,
                                         DefinitionKind::Function))) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  // Compile the module functions
  UniqueChars error;
  ModuleGenerator mg(*compileArgs, &moduleEnv, &compilerEnv, nullptr, &error,
                     nullptr);
  if (!mg.init(nullptr)) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Prepare and compile function bodies
  Vector<Bytes, 1, SystemAllocPolicy> bodies;
  if (!bodies.reserve(ids.size())) {
    ReportOutOfMemory(cx);
    return false;
  }
  for (uint32_t funcIndex = 0; funcIndex < ids.size(); funcIndex++) {
    BuiltinModuleFuncId id = ids[funcIndex];
    const BuiltinModuleFunc& builtinModuleFunc =
        BuiltinModuleFunc::getFromId(ids[funcIndex]);

    // Compilation may be done using other threads, ModuleGenerator requires
    // that function bodies live until after finishFuncDefs().
    bodies.infallibleAppend(Bytes());
    Bytes& bytecode = bodies.back();

    // Encode function body that will call the builtinModuleFunc using our
    // builtin opcode, and launch a compile task
    if (!EncodeFuncBody(builtinModuleFunc, id, &bytecode) ||
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

  // Create a dummy bytecode vector, that will not be used
  SharedBytes bytecode = js_new<ShareableBytes>();
  if (!bytecode) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Finish the module
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
