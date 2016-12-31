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

#include "jsnspr.h"

#include "jit/JitOptions.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmSerialize.h"

#include "jsatominlines.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/Debugger-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsNaN;

const char wasm::InstanceExportField[] = "exports";

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
// On MIPS, CodeLabels are instruction immediates so InternalLinks only
// patch instruction immediates.
LinkData::InternalLink::InternalLink(Kind kind)
{
    MOZ_ASSERT(kind == CodeLabel || kind == InstructionImmediate);
}

bool
LinkData::InternalLink::isRawPointerPatch()
{
    return false;
}
#else
// On the rest, CodeLabels are raw pointers so InternalLinks only patch
// raw pointers.
LinkData::InternalLink::InternalLink(Kind kind)
{
    MOZ_ASSERT(kind == CodeLabel || kind == RawPointer);
}

bool
LinkData::InternalLink::isRawPointerPatch()
{
    return true;
}
#endif

size_t
LinkData::SymbolicLinkArray::serializedSize() const
{
    size_t size = 0;
    for (const Uint32Vector& offsets : *this)
        size += SerializedPodVectorSize(offsets);
    return size;
}

uint8_t*
LinkData::SymbolicLinkArray::serialize(uint8_t* cursor) const
{
    for (const Uint32Vector& offsets : *this)
        cursor = SerializePodVector(cursor, offsets);
    return cursor;
}

const uint8_t*
LinkData::SymbolicLinkArray::deserialize(const uint8_t* cursor)
{
    for (Uint32Vector& offsets : *this) {
        cursor = DeserializePodVector(cursor, &offsets);
        if (!cursor)
            return nullptr;
    }
    return cursor;
}

size_t
LinkData::SymbolicLinkArray::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t size = 0;
    for (const Uint32Vector& offsets : *this)
        size += offsets.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

size_t
LinkData::serializedSize() const
{
    return sizeof(pod()) +
           SerializedPodVectorSize(internalLinks) +
           symbolicLinks.serializedSize();
}

uint8_t*
LinkData::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = SerializePodVector(cursor, internalLinks);
    cursor = symbolicLinks.serialize(cursor);
    return cursor;
}

const uint8_t*
LinkData::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod(), sizeof(pod()))) &&
    (cursor = DeserializePodVector(cursor, &internalLinks)) &&
    (cursor = symbolicLinks.deserialize(cursor));
    return cursor;
}

size_t
LinkData::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return internalLinks.sizeOfExcludingThis(mallocSizeOf) +
           symbolicLinks.sizeOfExcludingThis(mallocSizeOf);
}

/* virtual */ void
Module::serializedSize(size_t* maybeBytecodeSize, size_t* maybeCompiledSize) const
{
    if (maybeBytecodeSize)
        *maybeBytecodeSize = bytecode_->bytes.length();

    if (maybeCompiledSize) {
        *maybeCompiledSize = assumptions_.serializedSize() +
                             SerializedPodVectorSize(code_) +
                             linkData_.serializedSize() +
                             SerializedVectorSize(imports_) +
                             SerializedVectorSize(exports_) +
                             SerializedPodVectorSize(dataSegments_) +
                             SerializedVectorSize(elemSegments_) +
                             metadata_->serializedSize();
    }
}

/* virtual */ void
Module::serialize(uint8_t* maybeBytecodeBegin, size_t maybeBytecodeSize,
                  uint8_t* maybeCompiledBegin, size_t maybeCompiledSize) const
{
    MOZ_ASSERT(!!maybeBytecodeBegin == !!maybeBytecodeSize);
    MOZ_ASSERT(!!maybeCompiledBegin == !!maybeCompiledSize);

    if (maybeBytecodeBegin) {
        // Bytecode deserialization is not guarded by Assumptions and thus must not
        // change incompatibly between builds. Thus, for simplicity, the format
        // of the bytecode file is simply a .wasm file (thus, backwards
        // compatibility is ensured by backwards compatibility of the wasm
        // binary format).

        const Bytes& bytes = bytecode_->bytes;
        uint8_t* bytecodeEnd = WriteBytes(maybeBytecodeBegin, bytes.begin(), bytes.length());
        MOZ_RELEASE_ASSERT(bytecodeEnd == maybeBytecodeBegin + maybeBytecodeSize);
    }

    if (maybeCompiledBegin) {
        // Assumption must be serialized at the beginning of the compiled bytes so
        // that compiledAssumptionsMatch can detect a build-id mismatch before any
        // other decoding occurs.

        uint8_t* cursor = maybeCompiledBegin;
        cursor = assumptions_.serialize(cursor);
        cursor = SerializePodVector(cursor, code_);
        cursor = linkData_.serialize(cursor);
        cursor = SerializeVector(cursor, imports_);
        cursor = SerializeVector(cursor, exports_);
        cursor = SerializePodVector(cursor, dataSegments_);
        cursor = SerializeVector(cursor, elemSegments_);
        cursor = metadata_->serialize(cursor);
        MOZ_RELEASE_ASSERT(cursor == maybeCompiledBegin + maybeCompiledSize);
    }
}

/* static */ bool
Module::assumptionsMatch(const Assumptions& current, const uint8_t* compiledBegin, size_t remain)
{
    Assumptions cached;
    if (!cached.deserialize(compiledBegin, remain))
        return false;

    return current == cached;
}

/* static */ SharedModule
Module::deserialize(const uint8_t* bytecodeBegin, size_t bytecodeSize,
                    const uint8_t* compiledBegin, size_t compiledSize,
                    Metadata* maybeMetadata)
{
    MutableBytes bytecode = js_new<ShareableBytes>();
    if (!bytecode || !bytecode->bytes.initLengthUninitialized(bytecodeSize))
        return nullptr;

    memcpy(bytecode->bytes.begin(), bytecodeBegin, bytecodeSize);

    Assumptions assumptions;
    const uint8_t* cursor = assumptions.deserialize(compiledBegin, compiledSize);
    if (!cursor)
        return nullptr;

    Bytes code;
    cursor = DeserializePodVector(cursor, &code);
    if (!cursor)
        return nullptr;

    LinkData linkData;
    cursor = linkData.deserialize(cursor);
    if (!cursor)
        return nullptr;

    ImportVector imports;
    cursor = DeserializeVector(cursor, &imports);
    if (!cursor)
        return nullptr;

    ExportVector exports;
    cursor = DeserializeVector(cursor, &exports);
    if (!cursor)
        return nullptr;

    DataSegmentVector dataSegments;
    cursor = DeserializePodVector(cursor, &dataSegments);
    if (!cursor)
        return nullptr;

    ElemSegmentVector elemSegments;
    cursor = DeserializeVector(cursor, &elemSegments);
    if (!cursor)
        return nullptr;

    MutableMetadata metadata;
    if (maybeMetadata) {
        metadata = maybeMetadata;
    } else {
        metadata = js_new<Metadata>();
        if (!metadata)
            return nullptr;
    }
    cursor = metadata->deserialize(cursor);
    if (!cursor)
        return nullptr;

    MOZ_RELEASE_ASSERT(cursor == compiledBegin + compiledSize);
    MOZ_RELEASE_ASSERT(!!maybeMetadata == metadata->isAsmJS());

    return js_new<Module>(Move(assumptions),
                          Move(code),
                          Move(linkData),
                          Move(imports),
                          Move(exports),
                          Move(dataSegments),
                          Move(elemSegments),
                          *metadata,
                          *bytecode);
}

/* virtual */ JSObject*
Module::createObject(JSContext* cx)
{
    if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_WebAssembly))
        return nullptr;

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
    if (PR_GetOpenFileInfo(file, info) != PR_SUCCESS)
        return nullptr;

    PRFileMap* map = PR_CreateFileMap(file, info->size, PR_PROT_READONLY);
    if (!map)
        return nullptr;

    // PRFileMap objects do not need to be kept alive after the memory has been
    // mapped, so unconditionally close the PRFileMap, regardless of whether
    // PR_MemMap succeeds.
    uint8_t* memory = (uint8_t*)PR_MemMap(map, 0, info->size);
    PR_CloseFileMap(map);
    return UniqueMapping(memory, MemUnmap(info->size));
}

bool
wasm::CompiledModuleAssumptionsMatch(PRFileDesc* compiled, JS::BuildIdCharVector&& buildId)
{
    PRFileInfo info;
    UniqueMapping mapping = MapFile(compiled, &info);
    if (!mapping)
        return false;

    Assumptions assumptions(Move(buildId));
    return Module::assumptionsMatch(assumptions, mapping.get(), info.size);
}

SharedModule
wasm::DeserializeModule(PRFileDesc* bytecodeFile, PRFileDesc* maybeCompiledFile,
                        JS::BuildIdCharVector&& buildId, UniqueChars filename,
                        unsigned line, unsigned column)
{
    PRFileInfo bytecodeInfo;
    UniqueMapping bytecodeMapping = MapFile(bytecodeFile, &bytecodeInfo);
    if (!bytecodeMapping)
        return nullptr;

    if (PRFileDesc* compiledFile = maybeCompiledFile) {
        PRFileInfo compiledInfo;
        UniqueMapping compiledMapping = MapFile(compiledFile, &compiledInfo);
        if (!compiledMapping)
            return nullptr;

        return Module::deserialize(bytecodeMapping.get(), bytecodeInfo.size,
                                   compiledMapping.get(), compiledInfo.size);
    }

    // Since the compiled file's assumptions don't match, we must recompile from
    // bytecode. The bytecode file format is simply that of a .wasm (see
    // Module::serialize).

    MutableBytes bytecode = js_new<ShareableBytes>();
    if (!bytecode || !bytecode->bytes.initLengthUninitialized(bytecodeInfo.size))
        return nullptr;

    memcpy(bytecode->bytes.begin(), bytecodeMapping.get(), bytecodeInfo.size);

    ScriptedCaller scriptedCaller;
    scriptedCaller.filename = Move(filename);
    scriptedCaller.line = line;
    scriptedCaller.column = column;

    CompileArgs args(Assumptions(Move(buildId)), Move(scriptedCaller));

    UniqueChars error;
    return Compile(*bytecode, Move(args), &error);
}

/* virtual */ void
Module::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                      Metadata::SeenSet* seenMetadata,
                      ShareableBytes::SeenSet* seenBytes,
                      size_t* code,
                      size_t* data) const
{
    *data += mallocSizeOf(this) +
             assumptions_.sizeOfExcludingThis(mallocSizeOf) +
             code_.sizeOfExcludingThis(mallocSizeOf) +
             linkData_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(imports_, mallocSizeOf) +
             SizeOfVectorExcludingThis(exports_, mallocSizeOf) +
             dataSegments_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(elemSegments_, mallocSizeOf) +
             metadata_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenMetadata) +
             bytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
}


// Extracting machine code as JS object. The result has the "code" property, as
// a Uint8Array, and the "segments" property as array objects. The objects
// contain offsets in the "code" array and basic information about a code
// segment/function body.
bool
Module::extractCode(JSContext* cx, MutableHandleValue vp)
{
    RootedPlainObject result(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!result)
        return false;

    RootedObject code(cx, JS_NewUint8Array(cx, code_.length()));
    if (!code)
        return false;

    memcpy(code->as<TypedArrayObject>().viewDataUnshared(), code_.begin(), code_.length());

    RootedValue value(cx, ObjectValue(*code));
    if (!JS_DefineProperty(cx, result, "code", value, JSPROP_ENUMERATE))
        return false;

    RootedObject segments(cx, NewDenseEmptyArray(cx));
    if (!segments)
        return false;

    for (const CodeRange& p : metadata_->codeRanges) {
        RootedObject segment(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
        if (!segment)
            return false;

        value.setNumber((uint32_t)p.begin());
        if (!JS_DefineProperty(cx, segment, "begin", value, JSPROP_ENUMERATE))
            return false;

        value.setNumber((uint32_t)p.end());
        if (!JS_DefineProperty(cx, segment, "end", value, JSPROP_ENUMERATE))
            return false;

        value.setNumber((uint32_t)p.kind());
        if (!JS_DefineProperty(cx, segment, "kind", value, JSPROP_ENUMERATE))
            return false;

        if (p.isFunction()) {
            value.setNumber((uint32_t)p.funcIndex());
            if (!JS_DefineProperty(cx, segment, "funcIndex", value, JSPROP_ENUMERATE))
                return false;

            value.setNumber((uint32_t)p.funcNonProfilingEntry());
            if (!JS_DefineProperty(cx, segment, "funcBodyBegin", value, JSPROP_ENUMERATE))
                return false;

            value.setNumber((uint32_t)p.funcProfilingEpilogue());
            if (!JS_DefineProperty(cx, segment, "funcBodyEnd", value, JSPROP_ENUMERATE))
                return false;
        }

        if (!NewbornArrayPush(cx, segments, ObjectValue(*segment)))
            return false;
    }

    value.setObject(*segments);
    if (!JS_DefineProperty(cx, result, "segments", value, JSPROP_ENUMERATE))
        return false;

    vp.setObject(*result);
    return true;
}

static uint32_t
EvaluateInitExpr(const ValVector& globalImports, InitExpr initExpr)
{
    switch (initExpr.kind()) {
      case InitExpr::Kind::Constant:
        return initExpr.val().i32();
      case InitExpr::Kind::GetGlobal:
        return globalImports[initExpr.globalIndex()].i32();
    }

    MOZ_CRASH("bad initializer expression");
}

bool
Module::initSegments(JSContext* cx,
                     HandleWasmInstanceObject instanceObj,
                     Handle<FunctionVector> funcImports,
                     HandleWasmMemoryObject memoryObj,
                     const ValVector& globalImports) const
{
    Instance& instance = instanceObj->instance();
    const SharedTableVector& tables = instance.tables();

    // Perform all error checks up front so that this function does not perform
    // partial initialization if an error is reported.

    for (const ElemSegment& seg : elemSegments_) {
        uint32_t numElems = seg.elemCodeRangeIndices.length();
        if (!numElems)
            continue;

        uint32_t tableLength = tables[seg.tableIndex]->length();
        uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);

        if (offset > tableLength || tableLength - offset < numElems) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT,
                                      "elem", "table");
            return false;
        }
    }

    if (memoryObj) {
        for (const DataSegment& seg : dataSegments_) {
            if (!seg.length)
                continue;

            uint32_t memoryLength = memoryObj->buffer().byteLength();
            uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);

            if (offset > memoryLength || memoryLength - offset < seg.length) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT,
                                          "data", "memory");
                return false;
            }
        }
    } else {
        MOZ_ASSERT(dataSegments_.empty());
    }

    // Now that initialization can't fail partway through, write data/elem
    // segments into memories/tables.

    for (const ElemSegment& seg : elemSegments_) {
        Table& table = *tables[seg.tableIndex];
        uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);
        bool profilingEnabled = instance.code().profilingEnabled();
        const CodeRangeVector& codeRanges = metadata().codeRanges;
        uint8_t* codeBase = instance.codeBase();

        for (uint32_t i = 0; i < seg.elemCodeRangeIndices.length(); i++) {
            uint32_t funcIndex = seg.elemFuncIndices[i];
            if (funcIndex < funcImports.length() && IsExportedWasmFunction(funcImports[funcIndex])) {
                MOZ_ASSERT(!metadata().isAsmJS());
                MOZ_ASSERT(!table.isTypedFunction());

                HandleFunction f = funcImports[funcIndex];
                WasmInstanceObject* exportInstanceObj = ExportedFunctionToInstanceObject(f);
                const CodeRange& cr = exportInstanceObj->getExportedFunctionCodeRange(f);
                Instance& exportInstance = exportInstanceObj->instance();
                table.set(offset + i, exportInstance.codeBase() + cr.funcTableEntry(), exportInstance);
            } else {
                const CodeRange& cr = codeRanges[seg.elemCodeRangeIndices[i]];
                uint32_t entryOffset = table.isTypedFunction()
                                       ? profilingEnabled
                                         ? cr.funcProfilingEntry()
                                         : cr.funcNonProfilingEntry()
                                       : cr.funcTableEntry();
                table.set(offset + i, codeBase + entryOffset, instance);
            }
        }
    }

    if (memoryObj) {
        uint8_t* memoryBase = memoryObj->buffer().dataPointerEither().unwrap(/* memcpy */);

        for (const DataSegment& seg : dataSegments_) {
            MOZ_ASSERT(seg.bytecodeOffset <= bytecode_->length());
            MOZ_ASSERT(seg.length <= bytecode_->length() - seg.bytecodeOffset);
            uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);
            memcpy(memoryBase + offset, bytecode_->begin() + seg.bytecodeOffset, seg.length);
        }
    }

    return true;
}

static const Import&
FindImportForFuncImport(const ImportVector& imports, uint32_t funcImportIndex)
{
    for (const Import& import : imports) {
        if (import.kind != DefinitionKind::Function)
            continue;
        if (funcImportIndex == 0)
            return import;
        funcImportIndex--;
    }
    MOZ_CRASH("ran out of imports");
}

bool
Module::instantiateFunctions(JSContext* cx, Handle<FunctionVector> funcImports) const
{
    MOZ_ASSERT(funcImports.length() == metadata_->funcImports.length());

    if (metadata().isAsmJS())
        return true;

    for (size_t i = 0; i < metadata_->funcImports.length(); i++) {
        HandleFunction f = funcImports[i];
        if (!IsExportedFunction(f) || ExportedFunctionToInstance(f).isAsmJS())
            continue;

        uint32_t funcIndex = ExportedFunctionToFuncIndex(f);
        Instance& instance = ExportedFunctionToInstance(f);
        const FuncExport& funcExport = instance.metadata().lookupFuncExport(funcIndex);

        if (funcExport.sig() != metadata_->funcImports[i].sig()) {
            const Import& import = FindImportForFuncImport(imports_, i);
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_SIG,
                                      import.module.get(), import.field.get());
            return false;
        }
    }

    return true;
}

static bool
CheckLimits(JSContext* cx, uint32_t declaredMin, Maybe<uint32_t> declaredMax, uint32_t actualLength,
            Maybe<uint32_t> actualMax, bool isAsmJS, const char* kind)
{
    if (isAsmJS) {
        MOZ_ASSERT(actualLength >= declaredMin);
        MOZ_ASSERT(!declaredMax);
        MOZ_ASSERT(actualLength == actualMax.value());
        return true;
    }

    if (actualLength < declaredMin || actualLength > declaredMax.valueOr(UINT32_MAX)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_SIZE, kind);
        return false;
    }

    if ((actualMax && declaredMax && *actualMax > *declaredMax) || (!actualMax && declaredMax)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_MAX, kind);
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
    if (!metadata_->usesMemory()) {
        MOZ_ASSERT(!memory);
        MOZ_ASSERT(dataSegments_.empty());
        return true;
    }

    uint32_t declaredMin = metadata_->minMemoryLength;
    Maybe<uint32_t> declaredMax = metadata_->maxMemoryLength;

    if (memory) {
        ArrayBufferObjectMaybeShared& buffer = memory->buffer();
        MOZ_ASSERT_IF(metadata_->isAsmJS(), buffer.isPreparedForAsmJS());
        MOZ_ASSERT_IF(!metadata_->isAsmJS(), buffer.as<ArrayBufferObject>().isWasm());

        if (!CheckLimits(cx, declaredMin, declaredMax, buffer.byteLength(), buffer.wasmMaxSize(),
                         metadata_->isAsmJS(), "Memory")) {
            return false;
        }
    } else {
        MOZ_ASSERT(!metadata_->isAsmJS());
        MOZ_ASSERT(metadata_->memoryUsage == MemoryUsage::Unshared);

        RootedArrayBufferObjectMaybeShared buffer(cx,
            ArrayBufferObject::createForWasm(cx, declaredMin, declaredMax));
        if (!buffer)
            return false;

        RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());

        memory.set(WasmMemoryObject::create(cx, buffer, proto));
        if (!memory)
            return false;
    }

    return true;
}

bool
Module::instantiateTable(JSContext* cx, MutableHandleWasmTableObject tableObj,
                         SharedTableVector* tables) const
{
    if (tableObj) {
        MOZ_ASSERT(!metadata_->isAsmJS());

        MOZ_ASSERT(metadata_->tables.length() == 1);
        const TableDesc& td = metadata_->tables[0];
        MOZ_ASSERT(td.external);

        Table& table = tableObj->table();
        if (!CheckLimits(cx, td.limits.initial, td.limits.maximum, table.length(), table.maximum(),
                         metadata_->isAsmJS(), "Table")) {
            return false;
        }

        if (!tables->append(&table)) {
            ReportOutOfMemory(cx);
            return false;
        }
    } else {
        for (const TableDesc& td : metadata_->tables) {
            SharedTable table;
            if (td.external) {
                MOZ_ASSERT(!tableObj);
                MOZ_ASSERT(td.kind == TableKind::AnyFunction);

                tableObj.set(WasmTableObject::create(cx, td.limits));
                if (!tableObj)
                    return false;

                table = &tableObj->table();
            } else {
                table = Table::create(cx, td, /* HandleWasmTableObject = */ nullptr);
                if (!table)
                    return false;
            }

            if (!tables->emplaceBack(table)) {
                ReportOutOfMemory(cx);
                return false;
            }
        }
    }

    return true;
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
    if (!instanceObj->getExportedFunction(cx, instanceObj, exp.funcIndex(), &fun))
        return false;

    val.setObject(*fun);
    return true;
}

static bool
GetGlobalExport(JSContext* cx, const GlobalDescVector& globals, uint32_t globalIndex,
                const ValVector& globalImports, MutableHandleValue jsval)
{
    const GlobalDesc& global = globals[globalIndex];

    // Imports are located upfront in the globals array.
    Val val;
    switch (global.kind()) {
      case GlobalKind::Import:   val = globalImports[globalIndex]; break;
      case GlobalKind::Variable: MOZ_CRASH("mutable variables can't be exported");
      case GlobalKind::Constant: val = global.constantValue(); break;
    }

    switch (global.type()) {
      case ValType::I32: {
        jsval.set(Int32Value(val.i32()));
        return true;
      }
      case ValType::I64: {
        MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
        RootedObject obj(cx, CreateI64Object(cx, val.i64()));
        if (!obj)
            return false;
        jsval.set(ObjectValue(*obj));
        return true;
      }
      case ValType::F32: {
        float f = val.f32();
        if (JitOptions.wasmTestMode && IsNaN(f)) {
            RootedObject obj(cx, CreateCustomNaNObject(cx, &f));
            if (!obj)
                return false;
            jsval.set(ObjectValue(*obj));
            return true;
        }
        jsval.set(DoubleValue(double(f)));
        return true;
      }
      case ValType::F64: {
        double d = val.f64();
        if (JitOptions.wasmTestMode && IsNaN(d)) {
            RootedObject obj(cx, CreateCustomNaNObject(cx, &d));
            if (!obj)
                return false;
            jsval.set(ObjectValue(*obj));
            return true;
        }
        jsval.set(DoubleValue(d));
        return true;
      }
      default: {
        break;
      }
    }
    MOZ_CRASH("unexpected type when creating global exports");
}

static bool
CreateExportObject(JSContext* cx,
                   HandleWasmInstanceObject instanceObj,
                   Handle<FunctionVector> funcImports,
                   HandleWasmTableObject tableObj,
                   HandleWasmMemoryObject memoryObj,
                   const ValVector& globalImports,
                   const ExportVector& exports,
                   MutableHandleObject exportObj)
{
    const Instance& instance = instanceObj->instance();
    const Metadata& metadata = instance.metadata();

    if (metadata.isAsmJS() && exports.length() == 1 && strlen(exports[0].fieldName()) == 0) {
        RootedValue val(cx);
        if (!GetFunctionExport(cx, instanceObj, funcImports, exports[0], &val))
            return false;
        exportObj.set(&val.toObject());
        return true;
    }

    if (metadata.isAsmJS())
        exportObj.set(NewBuiltinClassInstance<PlainObject>(cx));
    else
        exportObj.set(NewObjectWithGivenProto<PlainObject>(cx, nullptr));
    if (!exportObj)
        return false;

    for (const Export& exp : exports) {
        JSAtom* atom = AtomizeUTF8Chars(cx, exp.fieldName(), strlen(exp.fieldName()));
        if (!atom)
            return false;

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        switch (exp.kind()) {
          case DefinitionKind::Function:
            if (!GetFunctionExport(cx, instanceObj, funcImports, exp, &val))
                return false;
            break;
          case DefinitionKind::Table:
            val = ObjectValue(*tableObj);
            break;
          case DefinitionKind::Memory:
            val = ObjectValue(*memoryObj);
            break;
          case DefinitionKind::Global:
            if (!GetGlobalExport(cx, metadata.globals, exp.globalIndex(), globalImports, &val))
                return false;
            break;
        }

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
            return false;
    }

    if (!metadata.isAsmJS()) {
        if (!JS_FreezeObject(cx, exportObj))
            return false;
    }

    return true;
}

bool
Module::instantiate(JSContext* cx,
                    Handle<FunctionVector> funcImports,
                    HandleWasmTableObject tableImport,
                    HandleWasmMemoryObject memoryImport,
                    const ValVector& globalImports,
                    HandleObject instanceProto,
                    MutableHandleWasmInstanceObject instance) const
{
    if (!instantiateFunctions(cx, funcImports))
        return false;

    RootedWasmMemoryObject memory(cx, memoryImport);
    if (!instantiateMemory(cx, &memory))
        return false;

    RootedWasmTableObject table(cx, tableImport);
    SharedTableVector tables;
    if (!instantiateTable(cx, &table, &tables))
        return false;

    // To support viewing the source of an instance (Instance::createText), the
    // instance must hold onto a ref of the bytecode (keeping it alive). This
    // wastes memory for most users, so we try to only save the source when a
    // developer actually cares: when the compartment is debuggable (which is
    // true when the web console is open) or a names section is present (since
    // this going to be stripped for non-developer builds).

    const ShareableBytes* maybeBytecode = nullptr;
    if (cx->compartment()->isDebuggee() || !metadata_->funcNames.empty())
        maybeBytecode = bytecode_.get();

    auto codeSegment = CodeSegment::create(cx, code_, linkData_, *metadata_, memory);
    if (!codeSegment)
        return false;

    auto code = cx->make_unique<Code>(Move(codeSegment), *metadata_, maybeBytecode);
    if (!code)
        return false;

    instance.set(WasmInstanceObject::create(cx,
                                            Move(code),
                                            memory,
                                            Move(tables),
                                            funcImports,
                                            globalImports,
                                            instanceProto));
    if (!instance)
        return false;

    RootedObject exportObj(cx);
    if (!CreateExportObject(cx, instance, funcImports, table, memory, globalImports, exports_, &exportObj))
        return false;

    JSAtom* atom = Atomize(cx, InstanceExportField, strlen(InstanceExportField));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));

    RootedValue val(cx, ObjectValue(*exportObj));
    if (!JS_DefinePropertyById(cx, instance, id, val, JSPROP_ENUMERATE))
        return false;

    // Register the instance with the JSCompartment so that it can find out
    // about global events like profiling being enabled in the compartment.
    // Registration does not require a fully-initialized instance and must
    // precede initSegments as the final pre-requisite for a live instance.

    if (!cx->compartment()->wasm.registerInstance(cx, instance))
        return false;

    // Perform initialization as the final step after the instance is fully
    // constructed since this can make the instance live to content (even if the
    // start function fails).

    if (!initSegments(cx, instance, funcImports, memory, globalImports))
        return false;

    // Now that the instance is fully live and initialized, the start function.
    // Note that failure may cause instantiation to throw, but the instance may
    // still be live via edges created by initSegments or the start function.

    if (metadata_->startFuncIndex) {
        FixedInvokeArgs<0> args(cx);
        if (!instance->instance().callExport(cx, *metadata_->startFuncIndex, args))
            return false;
    }

    uint32_t mode = uint32_t(metadata().isAsmJS() ? Telemetry::ASMJS : Telemetry::WASM);
    cx->runtime()->addTelemetry(JS_TELEMETRY_AOT_USAGE, mode);

    return true;
}
