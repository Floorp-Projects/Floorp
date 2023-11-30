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

#ifndef wasm_builtin_module_h
#define wasm_builtin_module_h

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"

#include "wasm/WasmBuiltins.h"
#include "wasm/WasmCompileArgs.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmTypeDecls.h"
#include "wasm/WasmTypeDef.h"

namespace js {
namespace wasm {

// An builtin module func is a natively implemented function that may be
// compiled into a 'builtin module', which may be instantiated with a provided
// memory yielding an exported WebAssembly function wrapping the builtin module.
struct BuiltinModuleFunc {
  // The name of the func as it is exported
  const char* exportName;
  // The params taken by the func.
  mozilla::Span<const ValType> params;
  // The optional result returned by the func.
  mozilla::Maybe<const ValType> result;
  // The signature of the builtin that implements the func
  const SymbolicAddressSignature& signature;
  // Whether this function takes a pointer to the memory base as a hidden final
  // parameter.
  bool usesMemory;

  // Allocate a FuncType for this func, returning false for OOM
  bool funcType(FuncType* type) const;

  // Get the BuiltinModuleFunc for an BuiltinModuleFuncId. BuiltinModuleFuncId
  // must be validated.
  static const BuiltinModuleFunc& getFromId(BuiltinModuleFuncId id);
};

// Compile and return the builtin module for a given set of operations.
bool CompileBuiltinModule(JSContext* cx,
                          const mozilla::Span<BuiltinModuleFuncId> ids,
                          mozilla::Maybe<Shareable> memory,
                          MutableHandle<WasmModuleObject*> result);

}  // namespace wasm
}  // namespace js

#endif  // wasm_builtin_module_h
