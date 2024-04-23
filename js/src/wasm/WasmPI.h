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

#ifndef wasm_pi_h
#define wasm_pi_h

#include "mozilla/DoublyLinkedList.h"  // for DoublyLinkedListElement

#include "js/TypeDecls.h"
#include "wasm/WasmTypeDef.h"

namespace js::wasm {

class SuspenderObject;

static const uint32_t SuspenderObjectDataSlot = 0;

enum SuspenderArgPosition {
  None = -1,
  First = 0,
  Last = 1,
};

enum SuspenderState {
  Initial,
  Moribund,
  Active,
  Suspended,
};

class SuspenderObjectData
    : public mozilla::DoublyLinkedListElement<SuspenderObjectData> {
  void* stackMemory_;

  // Stored main stack FP register.
  void* mainFP_;

  // Stored main stack SP register.
  void* mainSP_;

  // Stored suspendable stack FP register.
  void* suspendableFP_;

  // Stored suspendable stack SP register.
  void* suspendableSP_;

  // Stored suspendable stack exit/bottom frame pointer.
  void* suspendableExitFP_;

  // Stored return address for return to suspendable stack.
  void* suspendedReturnAddress_;

  SuspenderState state_;

#ifdef _WIN64
  // The storage of main stack limits during stack switching.
  // See updateTibFields and restoreTibFields below.
  void* savedStackBase_;
  void* savedStackLimit_;
#endif

 public:
  explicit SuspenderObjectData(void* stackMemory);

  inline SuspenderState state() const { return state_; }
  void setState(SuspenderState state) { state_ = state; }

  inline void* stackMemory() const { return stackMemory_; }
  inline void* mainFP() const { return mainFP_; }
  inline void* mainSP() const { return mainSP_; }
  inline void* suspendableFP() const { return suspendableFP_; }
  inline void* suspendableSP() const { return suspendableSP_; }
  inline void* suspendableExitFP() const { return suspendableExitFP_; }
  inline void* suspendedReturnAddress() const {
    return suspendedReturnAddress_;
  }

#ifdef _WIN64
  void updateTIBStackFields();
  void restoreTIBStackFields();
#endif

  static constexpr size_t offsetOfMainFP() {
    return offsetof(SuspenderObjectData, mainFP_);
  }

  static constexpr size_t offsetOfMainSP() {
    return offsetof(SuspenderObjectData, mainSP_);
  }

  static constexpr size_t offsetOfSuspendableFP() {
    return offsetof(SuspenderObjectData, suspendableFP_);
  }

  static constexpr size_t offsetOfSuspendableSP() {
    return offsetof(SuspenderObjectData, suspendableSP_);
  }

  static constexpr size_t offsetOfSuspendableExitFP() {
    return offsetof(SuspenderObjectData, suspendableExitFP_);
  }

  static constexpr size_t offsetOfSuspendedReturnAddress() {
    return offsetof(SuspenderObjectData, suspendedReturnAddress_);
  }
};

#ifdef ENABLE_WASM_JSPI

bool ParseSuspendingPromisingString(JSContext* cx, JS::HandleValue val,
                                    SuspenderArgPosition& result);

bool CallImportOnMainThread(JSContext* cx, Instance* instance,
                            int32_t funcImportIndex, int32_t argc,
                            uint64_t* argv);

bool IsAllowedOnSuspendableStack(HandleFunction fn);

void UnwindStackSwitch(JSContext* cx);

JSFunction* WasmSuspendingFunctionCreate(JSContext* cx, HandleObject func,
                                         wasm::ValTypeVector&& params,
                                         wasm::ValTypeVector&& results,
                                         SuspenderArgPosition argPosition);

JSFunction* WasmSuspendingFunctionCreate(JSContext* cx, HandleObject func,
                                         const FuncType& type);

JSFunction* WasmPromisingFunctionCreate(JSContext* cx, HandleObject func,
                                        wasm::ValTypeVector&& params,
                                        wasm::ValTypeVector&& results,
                                        SuspenderArgPosition argPosition);

#endif  // ENABLE_WASM_JSPI

void UpdateSuspenderState(Instance* instance, SuspenderObject* suspender,
                          UpdateSuspenderStateAction action);

}  // namespace js::wasm

#endif  // wasm_pi_h
