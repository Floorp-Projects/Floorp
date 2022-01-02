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

// This is an INTERNAL header for Wasm baseline compiler: inline BaseCompiler
// methods that don't fit in any other group in particular.

#ifndef wasm_wasm_baseline_object_inl_h
#define wasm_wasm_baseline_object_inl_h

namespace js {
namespace wasm {

const FuncType& BaseCompiler::funcType() const {
  return *moduleEnv_.funcs[func_.index].type;
}

const TypeIdDesc& BaseCompiler::funcTypeId() const {
  return *moduleEnv_.funcs[func_.index].typeId;
}

bool BaseCompiler::usesMemory() const { return moduleEnv_.usesMemory(); }

bool BaseCompiler::usesSharedMemory() const {
  return moduleEnv_.usesSharedMemory();
}

const Local& BaseCompiler::localFromSlot(uint32_t slot, MIRType type) {
  MOZ_ASSERT(localInfo_[slot].type == type);
  return localInfo_[slot];
}

uint32_t BaseCompiler::readCallSiteLineOrBytecode() {
  if (!func_.callSiteLineNums.empty()) {
    return func_.callSiteLineNums[lastReadCallSite_++];
  }
  return iter_.lastOpcodeOffset();
}

BytecodeOffset BaseCompiler::bytecodeOffset() const {
  return iter_.bytecodeOffset();
}

bool BaseCompiler::isMem32() const {
  return moduleEnv_.memory->indexType() == IndexType::I32;
}

bool BaseCompiler::isMem64() const {
  return moduleEnv_.memory->indexType() == IndexType::I64;
}

}  // namespace wasm
}  // namespace js

#endif  // wasm_wasm_baseline_object_inl_h
