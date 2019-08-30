/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef wasm_compile_h
#define wasm_compile_h

#include "wasm/WasmModule.h"

namespace js {
namespace wasm {

// Return a uint32_t which captures the observed properties of the CPU that
// affect compilation. If code compiled now is to be serialized and executed
// later, the ObservedCPUFeatures() must be ensured to be the same.

uint32_t ObservedCPUFeatures();

// Describes the JS scripted caller of a request to compile a wasm module.

struct ScriptedCaller {
  UniqueChars filename;
  bool filenameIsURL;
  unsigned line;

  ScriptedCaller() : filenameIsURL(false), line(0) {}
};

// Describes all the parameters that control wasm compilation.

struct CompileArgs;
typedef RefPtr<CompileArgs> MutableCompileArgs;
typedef RefPtr<const CompileArgs> SharedCompileArgs;

struct CompileArgs : ShareableBase<CompileArgs> {
  ScriptedCaller scriptedCaller;
  UniqueChars sourceMapURL;

  bool baselineEnabled;
  bool ionEnabled;
  bool craneliftEnabled;
  bool debugEnabled;
  bool sharedMemoryEnabled;
  bool forceTiering;
  bool gcEnabled;
  bool hugeMemory;

  // CompileArgs has two constructors:
  //
  // - one through a factory function `build`, which checks that flags are
  // consistent with each other.
  // - one that gives complete access to underlying fields.
  //
  // You should use the first one in general, unless you have a very good
  // reason (i.e. no JSContext around and you know which flags have been used).

  static SharedCompileArgs build(JSContext* cx,
                                 ScriptedCaller&& scriptedCaller);

  explicit CompileArgs(ScriptedCaller&& scriptedCaller)
      : scriptedCaller(std::move(scriptedCaller)),
        baselineEnabled(false),
        ionEnabled(false),
        craneliftEnabled(false),
        debugEnabled(false),
        sharedMemoryEnabled(false),
        forceTiering(false),
        gcEnabled(false),
        hugeMemory(false) {}
};

// Return the estimated compiled (machine) code size for the given bytecode size
// compiled at the given tier.

double EstimateCompiledCodeSize(Tier tier, size_t bytecodeSize);

// Compile the given WebAssembly bytecode with the given arguments into a
// wasm::Module. On success, the Module is returned. On failure, the returned
// SharedModule pointer is null and either:
//  - *error points to a string description of the error
//  - *error is null and the caller should report out-of-memory.

SharedModule CompileBuffer(const CompileArgs& args,
                           const ShareableBytes& bytecode, UniqueChars* error,
                           UniqueCharsVector* warnings,
                           JS::OptimizedEncodingListener* listener = nullptr);

// Attempt to compile the second tier of the given wasm::Module.

void CompileTier2(const CompileArgs& args, const Bytes& bytecode,
                  const Module& module, Atomic<bool>* cancelled);

// Compile the given WebAssembly module which has been broken into three
// partitions:
//  - envBytes contains a complete ModuleEnvironment that has already been
//    copied in from the stream.
//  - codeBytes is pre-sized to hold the complete code section when the stream
//    completes.
//  - The range [codeBytes.begin(), codeBytesEnd) contains the bytes currently
//    read from the stream and codeBytesEnd will advance until either
//    the stream is cancelled or codeBytesEnd == codeBytes.end().
//  - streamEnd contains the final information received after the code section:
//    the remaining module bytecodes and maybe a JS::OptimizedEncodingListener.
//    When the stream is successfully closed, streamEnd.reached is set.
// The ExclusiveWaitableData are notified when CompileStreaming() can make
// progress (i.e., codeBytesEnd advances or streamEnd.reached is set).
// If cancelled is set to true, compilation aborts and returns null. After
// cancellation is set, both ExclusiveWaitableData will be notified and so every
// wait() loop must check cancelled.

typedef ExclusiveWaitableData<const uint8_t*> ExclusiveBytesPtr;

struct StreamEndData {
  bool reached;
  const Bytes* tailBytes;
  Tier2Listener tier2Listener;

  StreamEndData() : reached(false) {}
};
typedef ExclusiveWaitableData<StreamEndData> ExclusiveStreamEndData;

SharedModule CompileStreaming(const CompileArgs& args, const Bytes& envBytes,
                              const Bytes& codeBytes,
                              const ExclusiveBytesPtr& codeBytesEnd,
                              const ExclusiveStreamEndData& streamEnd,
                              const Atomic<bool>& cancelled, UniqueChars* error,
                              UniqueCharsVector* warnings);

}  // namespace wasm
}  // namespace js

#endif  // namespace wasm_compile_h
