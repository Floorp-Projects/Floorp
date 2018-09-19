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

#include "wasm/WasmModule.h"

#include <chrono>
#include <thread>

#include "jit/JitOptions.h"
#include "threading/LockGuard.h"
#include "util/NSPR.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmSerialize.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/Debugger-inl.h"
#include "vm/JSAtom-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

class Module::Tier2GeneratorTaskImpl : public Tier2GeneratorTask
{
    SharedCompileArgs       compileArgs_;
    SharedBytes             bytecode_;
    SharedModule            module_;
    Atomic<bool>            cancelled_;

  public:
    Tier2GeneratorTaskImpl(const CompileArgs& compileArgs, const ShareableBytes& bytecode,
                           Module& module)
      : compileArgs_(&compileArgs),
        bytecode_(&bytecode),
        module_(&module),
        cancelled_(false)
    {}

    ~Tier2GeneratorTaskImpl() override {
        module_->testingTier2Active_ = false;
    }

    void cancel() override {
        cancelled_ = true;
    }

    void execute() override {
        CompileTier2(*compileArgs_, bytecode_->bytes, *module_, &cancelled_);
    }
};

void
Module::startTier2(const CompileArgs& args, const ShareableBytes& bytecode)
{
    MOZ_ASSERT(!testingTier2Active_);

    auto task = MakeUnique<Tier2GeneratorTaskImpl>(args, bytecode, *this);
    if (!task) {
        return;
    }

    // This flag will be cleared asynchronously by ~Tier2GeneratorTaskImpl()
    // on success or failure.
    testingTier2Active_ = true;

    StartOffThreadWasmTier2Generator(std::move(task));
}

bool
Module::finishTier2(const LinkData& linkData2, UniqueCodeTier code2) const
{
    MOZ_ASSERT(code().bestTier() == Tier::Baseline && code2->tier() == Tier::Ion);

    // Install the data in the data structures. They will not be visible
    // until commitTier2().

    if (!code().setTier2(std::move(code2), linkData2)) {
        return false;
    }

    // Before we can make tier-2 live, we need to compile tier2 versions of any
    // extant tier1 lazy stubs (otherwise, tiering would break the assumption
    // that any extant exported wasm function has had a lazy entry stub already
    // compiled for it).
    {
        // We need to prevent new tier1 stubs generation until we've committed
        // the newer tier2 stubs, otherwise we might not generate one tier2
        // stub that has been generated for tier1 before we committed.

        const MetadataTier& metadataTier1 = metadata(Tier::Baseline);

        auto stubs1 = code().codeTier(Tier::Baseline).lazyStubs().lock();
        auto stubs2 = code().codeTier(Tier::Ion).lazyStubs().lock();

        MOZ_ASSERT(stubs2->empty());

        Uint32Vector funcExportIndices;
        for (size_t i = 0; i < metadataTier1.funcExports.length(); i++) {
            const FuncExport& fe = metadataTier1.funcExports[i];
            if (fe.hasEagerStubs()) {
                continue;
            }
            if (!stubs1->hasStub(fe.funcIndex())) {
                continue;
            }
            if (!funcExportIndices.emplaceBack(i)) {
                return false;
            }
        }

        HasGcTypes gcTypesConfigured = code().metadata().temporaryGcTypesConfigured;
        const CodeTier& tier2 = code().codeTier(Tier::Ion);

        Maybe<size_t> stub2Index;
        if (!stubs2->createTier2(gcTypesConfigured, funcExportIndices, tier2, &stub2Index)) {
            return false;
        }

        // Now that we can't fail or otherwise abort tier2, make it live.

        MOZ_ASSERT(!code().hasTier2());
        code().commitTier2();

        stubs2->setJitEntries(stub2Index, code());
    }

    // And we update the jump vector.

    uint8_t* base = code().segment(Tier::Ion).base();
    for (const CodeRange& cr : metadata(Tier::Ion).codeRanges) {
        // These are racy writes that we just want to be visible, atomically,
        // eventually.  All hardware we care about will do this right.  But
        // we depend on the compiler not splitting the stores hidden inside the
        // set*Entry functions.
        if (cr.isFunction()) {
            code().setTieringEntry(cr.funcIndex(), base + cr.funcTierEntry());
        } else if (cr.isJitEntry()) {
            code().setJitEntry(cr.funcIndex(), base + cr.begin());
        }
    }

    return true;
}

void
Module::testingBlockOnTier2Complete() const
{
    while (testingTier2Active_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/* virtual */ size_t
Module::serializedSize(const LinkData& linkData) const
{
    return linkData.serializedSize() +
           SerializedVectorSize(imports_) +
           SerializedVectorSize(exports_) +
           SerializedVectorSize(structTypes_) +
           SerializedVectorSize(dataSegments_) +
           SerializedVectorSize(elemSegments_) +
           SerializedVectorSize(customSections_) +
           code_->serializedSize();
}

/* virtual */ void
Module::serialize(const LinkData& linkData, uint8_t* begin, size_t size) const
{
    MOZ_RELEASE_ASSERT(!testingTier2Active_);
    MOZ_RELEASE_ASSERT(!metadata().debugEnabled);
    MOZ_RELEASE_ASSERT(code_->hasTier(Tier::Serialized));

    uint8_t* cursor = begin;
    cursor = linkData.serialize(cursor);
    cursor = SerializeVector(cursor, imports_);
    cursor = SerializeVector(cursor, exports_);
    cursor = SerializeVector(cursor, structTypes_);
    cursor = SerializeVector(cursor, dataSegments_);
    cursor = SerializeVector(cursor, elemSegments_);
    cursor = SerializeVector(cursor, customSections_);
    cursor = code_->serialize(cursor, linkData);
    MOZ_RELEASE_ASSERT(cursor == begin + size);
}

/* static */ MutableModule
Module::deserialize(const uint8_t* begin, size_t size, Metadata* maybeMetadata)
{
    MutableMetadata metadata(maybeMetadata);
    if (!metadata) {
        metadata = js_new<Metadata>();
        if (!metadata) {
            return nullptr;
        }
    }

    const uint8_t* cursor = begin;

    LinkData linkData(Tier::Serialized);
    cursor = linkData.deserialize(cursor);
    if (!cursor) {
        return nullptr;
    }

    ImportVector imports;
    cursor = DeserializeVector(cursor, &imports);
    if (!cursor) {
        return nullptr;
    }

    ExportVector exports;
    cursor = DeserializeVector(cursor, &exports);
    if (!cursor) {
        return nullptr;
    }

    StructTypeVector structTypes;
    cursor = DeserializeVector(cursor, &structTypes);
    if (!cursor) {
        return nullptr;
    }

    DataSegmentVector dataSegments;
    cursor = DeserializeVector(cursor, &dataSegments);
    if (!cursor) {
        return nullptr;
    }

    ElemSegmentVector elemSegments;
    cursor = DeserializeVector(cursor, &elemSegments);
    if (!cursor) {
        return nullptr;
    }

    CustomSectionVector customSections;
    cursor = DeserializeVector(cursor, &customSections);
    if (!cursor) {
        return nullptr;
    }

    if (metadata->nameCustomSectionIndex) {
        metadata->namePayload = customSections[*metadata->nameCustomSectionIndex].payload;
    } else {
        MOZ_RELEASE_ASSERT(!metadata->moduleName);
        MOZ_RELEASE_ASSERT(metadata->funcNames.empty());
    }

    SharedCode code;
    cursor = Code::deserialize(cursor, linkData, *metadata, &code);
    if (!cursor) {
        return nullptr;
    }

    MOZ_RELEASE_ASSERT(cursor == begin + size);
    MOZ_RELEASE_ASSERT(!!maybeMetadata == code->metadata().isAsmJS());

    return js_new<Module>(*code,
                          std::move(imports),
                          std::move(exports),
                          std::move(structTypes),
                          std::move(dataSegments),
                          std::move(elemSegments),
                          std::move(customSections));
}

/* virtual */ JSObject*
Module::createObject(JSContext* cx)
{
    if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_WebAssembly)) {
        return nullptr;
    }

    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
    return WasmModuleObject::create(cx, *this, proto);
}

struct MemUnmap
{
    uint32_t size;
    MemUnmap() : size(0) {}
    explicit MemUnmap(uint32_t size) : size(size) {}
    void operator()(uint8_t* p) { MOZ_ASSERT(size); PR_MemUnmap(p, size); }
};

typedef UniquePtr<uint8_t, MemUnmap> UniqueMapping;

static UniqueMapping
MapFile(PRFileDesc* file, PRFileInfo* info)
{
    if (PR_GetOpenFileInfo(file, info) != PR_SUCCESS) {
        return nullptr;
    }

    PRFileMap* map = PR_CreateFileMap(file, info->size, PR_PROT_READONLY);
    if (!map) {
        return nullptr;
    }

    // PRFileMap objects do not need to be kept alive after the memory has been
    // mapped, so unconditionally close the PRFileMap, regardless of whether
    // PR_MemMap succeeds.
    uint8_t* memory = (uint8_t*)PR_MemMap(map, 0, info->size);
    PR_CloseFileMap(map);
    return UniqueMapping(memory, MemUnmap(info->size));
}

RefPtr<JS::WasmModule>
wasm::DeserializeModule(PRFileDesc* bytecodeFile, UniqueChars filename, unsigned line)
{
    PRFileInfo bytecodeInfo;
    UniqueMapping bytecodeMapping = MapFile(bytecodeFile, &bytecodeInfo);
    if (!bytecodeMapping) {
        return nullptr;
    }

    MutableBytes bytecode = js_new<ShareableBytes>();
    if (!bytecode || !bytecode->bytes.initLengthUninitialized(bytecodeInfo.size)) {
        return nullptr;
    }

    memcpy(bytecode->bytes.begin(), bytecodeMapping.get(), bytecodeInfo.size);

    ScriptedCaller scriptedCaller;
    scriptedCaller.filename = std::move(filename);
    scriptedCaller.line = line;

    MutableCompileArgs args = js_new<CompileArgs>(std::move(scriptedCaller));
    if (!args) {
        return nullptr;
    }

    // The true answer to whether shared memory is enabled is provided by
    // cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled()
    // where cx is the context that originated the call that caused this
    // deserialization attempt to happen.  We don't have that context here, so
    // we assume that shared memory is enabled; we will catch a wrong assumption
    // later, during instantiation.
    //
    // (We would prefer to store this value with the Assumptions when
    // serializing, and for the caller of the deserialization machinery to
    // provide the value from the originating context.)

    args->sharedMemoryEnabled = true;

    UniqueChars error;
    UniqueCharsVector warnings;
    SharedModule module = CompileBuffer(*args, *bytecode, &error, &warnings);
    if (!module) {
        return nullptr;
    }

    // The public interface is effectively const.
    return RefPtr<JS::WasmModule>(const_cast<Module*>(module.get()));
}

/* virtual */ void
Module::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                      Metadata::SeenSet* seenMetadata,
                      ShareableBytes::SeenSet* seenBytes,
                      Code::SeenSet* seenCode,
                      size_t* code,
                      size_t* data) const
{
    code_->addSizeOfMiscIfNotSeen(mallocSizeOf, seenMetadata, seenCode, code, data);
    *data += mallocSizeOf(this) +
             SizeOfVectorExcludingThis(imports_, mallocSizeOf) +
             SizeOfVectorExcludingThis(exports_, mallocSizeOf) +
             SizeOfVectorExcludingThis(structTypes_, mallocSizeOf) +
             SizeOfVectorExcludingThis(dataSegments_, mallocSizeOf) +
             SizeOfVectorExcludingThis(elemSegments_, mallocSizeOf) +
             SizeOfVectorExcludingThis(customSections_, mallocSizeOf);

    if (debugUnlinkedCode_) {
        *data += debugUnlinkedCode_->sizeOfExcludingThis(mallocSizeOf);
    }
}

// Extracting machine code as JS object. The result has the "code" property, as
// a Uint8Array, and the "segments" property as array objects. The objects
// contain offsets in the "code" array and basic information about a code
// segment/function body.
bool
Module::extractCode(JSContext* cx, Tier tier, MutableHandleValue vp) const
{
    RootedPlainObject result(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!result) {
        return false;
    }

    // This function is only used for testing purposes so we can simply
    // block on tiered compilation to complete.
    testingBlockOnTier2Complete();

    if (!code_->hasTier(tier)) {
        vp.setNull();
        return true;
    }

    const ModuleSegment& moduleSegment = code_->segment(tier);
    RootedObject code(cx, JS_NewUint8Array(cx, moduleSegment.length()));
    if (!code) {
        return false;
    }

    memcpy(code->as<TypedArrayObject>().viewDataUnshared(), moduleSegment.base(), moduleSegment.length());

    RootedValue value(cx, ObjectValue(*code));
    if (!JS_DefineProperty(cx, result, "code", value, JSPROP_ENUMERATE)) {
        return false;
    }

    RootedObject segments(cx, NewDenseEmptyArray(cx));
    if (!segments) {
        return false;
    }

    for (const CodeRange& p : metadata(tier).codeRanges) {
        RootedObject segment(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
        if (!segment) {
            return false;
        }

        value.setNumber((uint32_t)p.begin());
        if (!JS_DefineProperty(cx, segment, "begin", value, JSPROP_ENUMERATE)) {
            return false;
        }

        value.setNumber((uint32_t)p.end());
        if (!JS_DefineProperty(cx, segment, "end", value, JSPROP_ENUMERATE)) {
            return false;
        }

        value.setNumber((uint32_t)p.kind());
        if (!JS_DefineProperty(cx, segment, "kind", value, JSPROP_ENUMERATE)) {
            return false;
        }

        if (p.isFunction()) {
            value.setNumber((uint32_t)p.funcIndex());
            if (!JS_DefineProperty(cx, segment, "funcIndex", value, JSPROP_ENUMERATE)) {
                return false;
            }

            value.setNumber((uint32_t)p.funcNormalEntry());
            if (!JS_DefineProperty(cx, segment, "funcBodyBegin", value, JSPROP_ENUMERATE)) {
                return false;
            }

            value.setNumber((uint32_t)p.end());
            if (!JS_DefineProperty(cx, segment, "funcBodyEnd", value, JSPROP_ENUMERATE)) {
                return false;
            }
        }

        if (!NewbornArrayPush(cx, segments, ObjectValue(*segment))) {
            return false;
        }
    }

    value.setObject(*segments);
    if (!JS_DefineProperty(cx, result, "segments", value, JSPROP_ENUMERATE)) {
        return false;
    }

    vp.setObject(*result);
    return true;
}

static uint32_t
EvaluateInitExpr(HandleValVector globalImportValues, InitExpr initExpr)
{
    switch (initExpr.kind()) {
      case InitExpr::Kind::Constant:
        return initExpr.val().i32();
      case InitExpr::Kind::GetGlobal:
        return globalImportValues[initExpr.globalIndex()].get().i32();
    }

    MOZ_CRASH("bad initializer expression");
}

bool
Module::initSegments(JSContext* cx,
                     HandleWasmInstanceObject instanceObj,
                     Handle<FunctionVector> funcImports,
                     HandleWasmMemoryObject memoryObj,
                     HandleValVector globalImportValues) const
{
    Instance& instance = instanceObj->instance();
    const SharedTableVector& tables = instance.tables();

    // Perform all error checks up front so that this function does not perform
    // partial initialization if an error is reported.

    for (const ElemSegment* seg : elemSegments_) {
        if (!seg->active())
            continue;

        uint32_t tableLength = tables[seg->tableIndex]->length();
        uint32_t offset = EvaluateInitExpr(globalImportValues, seg->offset());

        if (offset > tableLength || tableLength - offset < seg->length()) {
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT,
                                     "elem", "table");
            return false;
        }
    }

    if (memoryObj) {
        uint32_t memoryLength = memoryObj->volatileMemoryLength();
        for (const DataSegment* seg : dataSegments_) {
            if (!seg->active())
                continue;

            uint32_t offset = EvaluateInitExpr(globalImportValues, seg->offset());

            if (offset > memoryLength || memoryLength - offset < seg->bytes.length()) {
                JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT,
                                         "data", "memory");
                return false;
            }
        }
    } else {
        MOZ_ASSERT(dataSegments_.empty());
    }

    // Now that initialization can't fail partway through, write data/elem
    // segments into memories/tables.

    for (const ElemSegment* seg : elemSegments_) {
        if (seg->active()) {
            uint32_t offset = EvaluateInitExpr(globalImportValues, seg->offset());
            instance.initElems(*seg, offset, 0, seg->length());
        }
    }

    if (memoryObj) {
        uint8_t* memoryBase = memoryObj->buffer().dataPointerEither().unwrap(/* memcpy */);

        for (const DataSegment* seg : dataSegments_) {
            if (!seg->active())
                continue;

            // But apply active segments right now.
            uint32_t offset = EvaluateInitExpr(globalImportValues, seg->offset());
            memcpy(memoryBase + offset, seg->bytes.begin(), seg->bytes.length());
        }
    } else {
        MOZ_ASSERT(dataSegments_.empty());
    }

    return true;
}

static const Import&
FindImportForFuncImport(const ImportVector& imports, uint32_t funcImportIndex)
{
    for (const Import& import : imports) {
        if (import.kind != DefinitionKind::Function) {
            continue;
        }
        if (funcImportIndex == 0) {
            return import;
        }
        funcImportIndex--;
    }
    MOZ_CRASH("ran out of imports");
}

bool
Module::instantiateFunctions(JSContext* cx, Handle<FunctionVector> funcImports) const
{
#ifdef DEBUG
    for (auto t : code().tiers()) {
        MOZ_ASSERT(funcImports.length() == metadata(t).funcImports.length());
    }
#endif

    if (metadata().isAsmJS()) {
        return true;
    }

    Tier tier = code().stableTier();

    for (size_t i = 0; i < metadata(tier).funcImports.length(); i++) {
        HandleFunction f = funcImports[i];
        if (!IsExportedFunction(f) || ExportedFunctionToInstance(f).isAsmJS()) {
            continue;
        }

        uint32_t funcIndex = ExportedFunctionToFuncIndex(f);
        Instance& instance = ExportedFunctionToInstance(f);
        Tier otherTier = instance.code().stableTier();

        const FuncExport& funcExport = instance.metadata(otherTier).lookupFuncExport(funcIndex);

        if (funcExport.funcType() != metadata(tier).funcImports[i].funcType()) {
            const Import& import = FindImportForFuncImport(imports_, i);
            JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_SIG,
                                     import.module.get(), import.field.get());
            return false;
        }
    }

    return true;
}

static bool
CheckLimits(JSContext* cx, uint32_t declaredMin, const Maybe<uint32_t>& declaredMax, uint32_t actualLength,
            const Maybe<uint32_t>& actualMax, bool isAsmJS, const char* kind)
{
    if (isAsmJS) {
        MOZ_ASSERT(actualLength >= declaredMin);
        MOZ_ASSERT(!declaredMax);
        MOZ_ASSERT(actualLength == actualMax.value());
        return true;
    }

    if (actualLength < declaredMin || actualLength > declaredMax.valueOr(UINT32_MAX)) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_SIZE, kind);
        return false;
    }

    if ((actualMax && declaredMax && *actualMax > *declaredMax) || (!actualMax && declaredMax)) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_MAX, kind);
        return false;
    }

    return true;
}

static bool
CheckSharing(JSContext* cx, bool declaredShared, bool isShared)
{
    if (isShared && !cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_NO_SHMEM_LINK);
        return false;
    }

    if (declaredShared && !isShared) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_IMP_SHARED_REQD);
        return false;
    }

    if (!declaredShared && isShared) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_IMP_SHARED_BANNED);
        return false;
    }

    return true;
}

// asm.js module instantiation supplies its own buffer, but for wasm, create and
// initialize the buffer if one is requested. Either way, the buffer is wrapped
// in a WebAssembly.Memory object which is what the Instance stores.
bool
Module::instantiateMemory(JSContext* cx, MutableHandleWasmMemoryObject memory) const
{
    if (!metadata().usesMemory()) {
        MOZ_ASSERT(!memory);
        MOZ_ASSERT(dataSegments_.empty());
        return true;
    }

    uint32_t declaredMin = metadata().minMemoryLength;
    Maybe<uint32_t> declaredMax = metadata().maxMemoryLength;
    bool declaredShared = metadata().memoryUsage == MemoryUsage::Shared;

    if (memory) {
        MOZ_ASSERT_IF(metadata().isAsmJS(), memory->buffer().isPreparedForAsmJS());
        MOZ_ASSERT_IF(!metadata().isAsmJS(), memory->buffer().isWasm());

        if (!CheckLimits(cx, declaredMin, declaredMax, memory->volatileMemoryLength(),
                         memory->buffer().wasmMaxSize(), metadata().isAsmJS(), "Memory"))
        {
            return false;
        }

        if (!CheckSharing(cx, declaredShared, memory->isShared())) {
            return false;
        }
    } else {
        MOZ_ASSERT(!metadata().isAsmJS());

        RootedArrayBufferObjectMaybeShared buffer(cx);
        Limits l(declaredMin,
                 declaredMax,
                 declaredShared ? Shareable::True : Shareable::False);
        if (!CreateWasmBuffer(cx, l, &buffer)) {
            return false;
        }

        RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());
        memory.set(WasmMemoryObject::create(cx, buffer, proto));
        if (!memory) {
            return false;
        }
    }

    return true;
}

bool
Module::instantiateTable(JSContext* cx, MutableHandleWasmTableObject tableObj,
                         SharedTableVector* tables) const
{
    if (tableObj) {
        MOZ_ASSERT(!metadata().isAsmJS());

        MOZ_ASSERT(metadata().tables.length() == 1);
        const TableDesc& td = metadata().tables[0];
        MOZ_ASSERT(td.external);

        Table& table = tableObj->table();
        if (!CheckLimits(cx, td.limits.initial, td.limits.maximum, table.length(), table.maximum(),
                         metadata().isAsmJS(), "Table")) {
            return false;
        }

        if (!tables->append(&table)) {
            ReportOutOfMemory(cx);
            return false;
        }
    } else {
        for (const TableDesc& td : metadata().tables) {
            SharedTable table;
            if (td.external) {
                MOZ_ASSERT(!tableObj);
                MOZ_ASSERT(td.kind == TableKind::AnyFunction);

                tableObj.set(WasmTableObject::create(cx, td.limits));
                if (!tableObj) {
                    return false;
                }

                table = &tableObj->table();
            } else {
                table = Table::create(cx, td, /* HandleWasmTableObject = */ nullptr);
                if (!table) {
                    return false;
                }
            }

            if (!tables->emplaceBack(table)) {
                ReportOutOfMemory(cx);
                return false;
            }
        }
    }

    return true;
}

static void
ExtractGlobalValue(HandleValVector globalImportValues, uint32_t globalIndex,
                   const GlobalDesc& global, MutableHandleVal result)
{
    switch (global.kind()) {
      case GlobalKind::Import: {
        result.set(Val(globalImportValues[globalIndex]));
        return;
      }
      case GlobalKind::Variable: {
        const InitExpr& init = global.initExpr();
        switch (init.kind()) {
          case InitExpr::Kind::Constant:
            result.set(Val(init.val()));
            return;
          case InitExpr::Kind::GetGlobal:
            result.set(Val(globalImportValues[init.globalIndex()]));
            return;
        }
        break;
      }
      case GlobalKind::Constant: {
        result.set(Val(global.constantValue()));
        return;
      }
    }
    MOZ_CRASH("Not a global value");
}

static bool
EnsureGlobalObject(JSContext* cx, HandleValVector globalImportValues, size_t globalIndex,
                   const GlobalDesc& global, WasmGlobalObjectVector& globalObjs)
{
    if (globalIndex < globalObjs.length() && globalObjs[globalIndex]) {
        return true;
    }

    RootedVal val(cx);
    ExtractGlobalValue(globalImportValues, globalIndex, global, &val);
    RootedWasmGlobalObject go(cx, WasmGlobalObject::create(cx, val, global.isMutable()));
    if (!go) {
        return false;
    }

    if (globalObjs.length() <= globalIndex && !globalObjs.resize(globalIndex + 1)) {
        ReportOutOfMemory(cx);
        return false;
    }

    globalObjs[globalIndex] = go;
    return true;
}

bool
Module::instantiateGlobals(JSContext* cx, HandleValVector globalImportValues,
                           WasmGlobalObjectVector& globalObjs) const
{
    // If there are exported globals that aren't in globalObjs because they
    // originate in this module or because they were immutable imports that came
    // in as primitive values then we must create cells in the globalObjs for
    // them here, as WasmInstanceObject::create() and CreateExportObject() will
    // need the cells to exist.

    const GlobalDescVector& globals = metadata().globals;

    for (const Export& exp : exports_) {
        if (exp.kind() != DefinitionKind::Global) {
            continue;
        }
        unsigned globalIndex = exp.globalIndex();
        const GlobalDesc& global = globals[globalIndex];
        if (!EnsureGlobalObject(cx, globalImportValues, globalIndex, global, globalObjs)) {
            return false;
        }
    }

    // Imported globals that are not re-exported may also have received only a
    // primitive value; these globals are always immutable.  Assert that we do
    // not need to create any additional Global objects for such imports.

#ifdef DEBUG
    size_t numGlobalImports = 0;
    for (const Import& import : imports_) {
        if (import.kind != DefinitionKind::Global) {
            continue;
        }
        size_t globalIndex = numGlobalImports++;
        const GlobalDesc& global = globals[globalIndex];
        MOZ_ASSERT(global.importIndex() == globalIndex);
        MOZ_ASSERT_IF(global.isIndirect(),
                      globalIndex < globalObjs.length() || globalObjs[globalIndex]);
    }
    MOZ_ASSERT_IF(!metadata().isAsmJS(),
                  numGlobalImports == globals.length() || !globals[numGlobalImports].isImport());
#endif
    return true;
}

SharedCode
Module::getDebugEnabledCode() const
{
    MOZ_ASSERT(metadata().debugEnabled);
    MOZ_ASSERT(debugUnlinkedCode_);
    MOZ_ASSERT(debugLinkData_);

    // The first time through, use the pre-linked code in the module but
    // mark it as having been claimed. Subsequently, instantiate the copy of the
    // code bytes that we keep around for debugging instead, because the
    // debugger may patch the pre-linked code at any time.
    if (debugCodeClaimed_.compareExchange(false, true)) {
        return code_;
    }

    Tier tier = Tier::Baseline;
    auto segment = ModuleSegment::create(tier, *debugUnlinkedCode_, *debugLinkData_);
    if (!segment) {
        return nullptr;
    }

    UniqueMetadataTier metadataTier = js::MakeUnique<MetadataTier>(tier);
    if (!metadataTier || !metadataTier->clone(metadata(tier))) {
        return nullptr;
    }

    auto codeTier = js::MakeUnique<CodeTier>(std::move(metadataTier), std::move(segment));
    if (!codeTier) {
        return nullptr;
    }

    JumpTables jumpTables;
    if (!jumpTables.init(CompileMode::Once, codeTier->segment(), metadata(tier).codeRanges)) {
        return nullptr;
    }

    MutableCode debugCode = js_new<Code>(std::move(codeTier), metadata(), std::move(jumpTables));
    if (!debugCode || !debugCode->initialize(*debugLinkData_)) {
        return nullptr;
    }

    return debugCode;
}

static bool
GetFunctionExport(JSContext* cx,
                  HandleWasmInstanceObject instanceObj,
                  Handle<FunctionVector> funcImports,
                  const Export& exp,
                  MutableHandleValue val)
{
    if (exp.funcIndex() < funcImports.length() &&
        IsExportedWasmFunction(funcImports[exp.funcIndex()]))
    {
        val.setObject(*funcImports[exp.funcIndex()]);
        return true;
    }

    RootedFunction fun(cx);
    if (!instanceObj->getExportedFunction(cx, instanceObj, exp.funcIndex(), &fun)) {
        return false;
    }

    val.setObject(*fun);
    return true;
}

static bool
CreateExportObject(JSContext* cx,
                   HandleWasmInstanceObject instanceObj,
                   Handle<FunctionVector> funcImports,
                   HandleWasmTableObject tableObj,
                   HandleWasmMemoryObject memoryObj,
                   const WasmGlobalObjectVector& globalObjs,
                   const ExportVector& exports)
{
    const Instance& instance = instanceObj->instance();
    const Metadata& metadata = instance.metadata();

    if (metadata.isAsmJS() && exports.length() == 1 && strlen(exports[0].fieldName()) == 0) {
        RootedValue val(cx);
        if (!GetFunctionExport(cx, instanceObj, funcImports, exports[0], &val)) {
            return false;
        }
        instanceObj->initExportsObj(val.toObject());
        return true;
    }

    RootedObject exportObj(cx);
    if (metadata.isAsmJS()) {
        exportObj = NewBuiltinClassInstance<PlainObject>(cx);
    } else {
        exportObj = NewObjectWithGivenProto<PlainObject>(cx, nullptr);
    }
    if (!exportObj) {
        return false;
    }

    for (const Export& exp : exports) {
        JSAtom* atom = AtomizeUTF8Chars(cx, exp.fieldName(), strlen(exp.fieldName()));
        if (!atom) {
            return false;
        }

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        switch (exp.kind()) {
          case DefinitionKind::Function:
            if (!GetFunctionExport(cx, instanceObj, funcImports, exp, &val)) {
                return false;
            }
            break;
          case DefinitionKind::Table:
            val = ObjectValue(*tableObj);
            break;
          case DefinitionKind::Memory:
            val = ObjectValue(*memoryObj);
            break;
          case DefinitionKind::Global:
            val.setObject(*globalObjs[exp.globalIndex()]);
            break;
        }

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE)) {
            return false;
        }
    }

    if (!metadata.isAsmJS()) {
        if (!JS_FreezeObject(cx, exportObj)) {
            return false;
        }
    }

    instanceObj->initExportsObj(*exportObj);
    return true;
}

bool
Module::instantiate(JSContext* cx,
                    Handle<FunctionVector> funcImports,
                    HandleWasmTableObject tableImport,
                    HandleWasmMemoryObject memoryImport,
                    HandleValVector globalImportValues,
                    WasmGlobalObjectVector& globalObjs,
                    HandleObject instanceProto,
                    MutableHandleWasmInstanceObject instance) const
{
    if (!instantiateFunctions(cx, funcImports)) {
        return false;
    }

    RootedWasmMemoryObject memory(cx, memoryImport);
    if (!instantiateMemory(cx, &memory)) {
        return false;
    }

    RootedWasmTableObject table(cx, tableImport);
    SharedTableVector tables;
    if (!instantiateTable(cx, &table, &tables)) {
        return false;
    }

    if (!instantiateGlobals(cx, globalImportValues, globalObjs)) {
        return false;
    }

    UniqueTlsData tlsData = CreateTlsData(metadata().globalDataLength);
    if (!tlsData) {
        ReportOutOfMemory(cx);
        return false;
    }

    SharedCode code;
    UniqueDebugState maybeDebug;
    if (metadata().debugEnabled) {
        code = getDebugEnabledCode();
        if (!code) {
            ReportOutOfMemory(cx);
            return false;
        }

        bool binarySource = cx->realm()->debuggerObservesBinarySource();
        maybeDebug = cx->make_unique<DebugState>(*code, *this, binarySource);
        if (!maybeDebug) {
            return false;
        }
    } else {
        code = code_;
    }

    instance.set(WasmInstanceObject::create(cx,
                                            code,
                                            dataSegments_,
                                            elemSegments_,
                                            std::move(tlsData),
                                            memory,
                                            std::move(tables),
                                            funcImports,
                                            metadata().globals,
                                            globalImportValues,
                                            globalObjs,
                                            instanceProto,
                                            std::move(maybeDebug)));
    if (!instance) {
        return false;
    }

    if (!CreateExportObject(cx, instance, funcImports, table, memory, globalObjs, exports_)) {
        return false;
    }

    // Register the instance with the Realm so that it can find out about global
    // events like profiling being enabled in the realm. Registration does not
    // require a fully-initialized instance and must precede initSegments as the
    // final pre-requisite for a live instance.

    if (!cx->realm()->wasm.registerInstance(cx, instance)) {
        return false;
    }

    // Perform initialization as the final step after the instance is fully
    // constructed since this can make the instance live to content (even if the
    // start function fails).

    if (!initSegments(cx, instance, funcImports, memory, globalImportValues)) {
        return false;
    }

    // Now that the instance is fully live and initialized, the start function.
    // Note that failure may cause instantiation to throw, but the instance may
    // still be live via edges created by initSegments or the start function.

    if (metadata().startFuncIndex) {
        FixedInvokeArgs<0> args(cx);
        if (!instance->instance().callExport(cx, *metadata().startFuncIndex, args)) {
            return false;
        }
    }

    JSUseCounter useCounter = metadata().isAsmJS() ? JSUseCounter::ASMJS : JSUseCounter::WASM;
    cx->runtime()->setUseCounter(instance, useCounter);

    if (cx->options().testWasmAwaitTier2()) {
        testingBlockOnTier2Complete();
    }

    return true;
}
