/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#ifndef wasm_module_h
#define wasm_module_h

#include "jit/shared/Assembler-shared.h"
#include "js/TypeDecls.h"
#include "threading/ConditionVariable.h"
#include "threading/Mutex.h"
#include "vm/MutexIDs.h"
#include "wasm/WasmCode.h"
#include "wasm/WasmTable.h"
#include "wasm/WasmValidate.h"

namespace js {
namespace wasm {

struct CompileArgs;

// Module represents a compiled wasm module and primarily provides two
// operations: instantiation and serialization. A Module can be instantiated any
// number of times to produce new Instance objects. A Module can be serialized
// any number of times such that the serialized bytes can be deserialized later
// to produce a new, equivalent Module.
//
// Fully linked-and-instantiated code (represented by Code and its owned
// ModuleSegment) can be shared between instances, provided none of those
// instances are being debugged. If patchable code is needed then each instance
// must have its own Code. Module eagerly creates a new Code and gives it to the
// first instance; it then instantiates new Code objects from a copy of the
// unlinked code that it keeps around for that purpose.

class Module : public JS::WasmModule
{
    const SharedCode        code_;
    const ImportVector      imports_;
    const ExportVector      exports_;
    const StructTypeVector  structTypes_;
    const DataSegmentVector dataSegments_;
    const ElemSegmentVector elemSegments_;
    const SharedBytes       bytecode_;

    // These fields are only meaningful when code_->metadata().debugEnabled.
    // `debugCodeClaimed_` is set to false initially and then to true when
    // `code_` is already being used for an instance and can't be shared because
    // it may be patched by the debugger. Subsequent instances must then create
    // copies by linking the `debugUnlinkedCode_` using `debugLinkData_`.
    // This could all be removed if debugging didn't need to perform
    // per-instance code patching.

    mutable Atomic<bool>    debugCodeClaimed_;
    const UniqueConstBytes  debugUnlinkedCode_;
    const UniqueLinkData    debugLinkData_;

    // This flag is only used for testing purposes and is racily updated as soon
    // as tier-2 compilation finishes (in success or failure).

    mutable Atomic<bool>    testingTier2Active_;

    bool instantiateFunctions(JSContext* cx, Handle<FunctionVector> funcImports) const;
    bool instantiateMemory(JSContext* cx, MutableHandleWasmMemoryObject memory) const;
    bool instantiateTable(JSContext* cx,
                          MutableHandleWasmTableObject table,
                          SharedTableVector* tables) const;
    bool instantiateGlobals(JSContext* cx, HandleValVector globalImportValues,
                            WasmGlobalObjectVector& globalObjs) const;
    bool initSegments(JSContext* cx,
                      HandleWasmInstanceObject instance,
                      Handle<FunctionVector> funcImports,
                      HandleWasmMemoryObject memory,
                      HandleValVector globalImportValues) const;
    SharedCode getDebugEnabledCode() const;

    class Tier2GeneratorTaskImpl;

  public:
    Module(const Code& code,
           ImportVector&& imports,
           ExportVector&& exports,
           StructTypeVector&& structTypes,
           DataSegmentVector&& dataSegments,
           ElemSegmentVector&& elemSegments,
           const ShareableBytes& bytecode,
           UniqueConstBytes debugUnlinkedCode = nullptr,
           UniqueLinkData debugLinkData = nullptr)
      : code_(&code),
        imports_(std::move(imports)),
        exports_(std::move(exports)),
        structTypes_(std::move(structTypes)),
        dataSegments_(std::move(dataSegments)),
        elemSegments_(std::move(elemSegments)),
        bytecode_(&bytecode),
        debugCodeClaimed_(false),
        debugUnlinkedCode_(std::move(debugUnlinkedCode)),
        debugLinkData_(std::move(debugLinkData)),
        testingTier2Active_(false)
    {
        MOZ_ASSERT_IF(metadata().debugEnabled, debugUnlinkedCode_ && debugLinkData_);
    }
    ~Module() override { /* Note: can be called on any thread */ }

    const Code& code() const { return *code_; }
    const ModuleSegment& moduleSegment(Tier t) const { return code_->segment(t); }
    const Metadata& metadata() const { return code_->metadata(); }
    const MetadataTier& metadata(Tier t) const { return code_->metadata(t); }
    const ImportVector& imports() const { return imports_; }
    const ExportVector& exports() const { return exports_; }
    const ShareableBytes& bytecode() const { return *bytecode_; }
    uint32_t codeLength(Tier t) const { return code_->segment(t).length(); }

    // Instantiate this module with the given imports:

    bool instantiate(JSContext* cx,
                     Handle<FunctionVector> funcImports,
                     HandleWasmTableObject tableImport,
                     HandleWasmMemoryObject memoryImport,
                     HandleValVector globalImportValues,
                     WasmGlobalObjectVector& globalObjs,
                     HandleObject instanceProto,
                     MutableHandleWasmInstanceObject instanceObj) const;

    // Tier-2 compilation may be initiated after the Module is constructed at
    // most once. When tier-2 compilation completes, ModuleGenerator calls
    // finishTier2() from a helper thread, passing tier-variant data which will
    // be installed and made visible.

    void startTier2(const CompileArgs& args);
    bool finishTier2(const LinkData& linkData2, UniqueCodeTier code2);

    void testingBlockOnTier2Complete() const;
    bool testingTier2Active() const { return testingTier2Active_; }

    // Currently dead, but will be ressurrected with shell tests (bug 1330661)
    // and HTTP cache integration.

    size_t serializedSize(const LinkData& linkData) const;
    void serialize(const LinkData& linkData, uint8_t* begin, size_t size) const;
    static RefPtr<Module> deserialize(const uint8_t* begin, size_t size,
                                      Metadata* maybeMetadata = nullptr);

    // JS API and JS::WasmModule implementation:

    JSObject* createObject(JSContext* cx) override;

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       Code::SeenSet* seenCode,
                       size_t* code, size_t* data) const;

    // Generated code analysis support:

    bool extractCode(JSContext* cx, Tier tier, MutableHandleValue vp) const;
};

typedef RefPtr<Module> SharedModule;

// JS API implementations:

SharedModule
DeserializeModule(PRFileDesc* bytecode, UniqueChars filename, unsigned line);

} // namespace wasm
} // namespace js

#endif // wasm_module_h
