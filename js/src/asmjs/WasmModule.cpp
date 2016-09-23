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

#include "asmjs/WasmModule.h"

#include "asmjs/WasmInstance.h"
#include "asmjs/WasmJS.h"
#include "asmjs/WasmSerialize.h"
#include "jit/JitOptions.h"

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

size_t
Import::serializedSize() const
{
    return module.serializedSize() +
           func.serializedSize();
}

uint8_t*
Import::serialize(uint8_t* cursor) const
{
    cursor = module.serialize(cursor);
    cursor = func.serialize(cursor);
    return cursor;
}

const uint8_t*
Import::deserialize(const uint8_t* cursor)
{
    (cursor = module.deserialize(cursor)) &&
    (cursor = func.deserialize(cursor));
    return cursor;
}

size_t
Import::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return module.sizeOfExcludingThis(mallocSizeOf) +
           func.sizeOfExcludingThis(mallocSizeOf);
}

Export::Export(UniqueChars fieldName, uint32_t index, DefinitionKind kind)
  : fieldName_(Move(fieldName))
{
    pod.kind_ = kind;
    pod.index_ = index;
}

Export::Export(UniqueChars fieldName, DefinitionKind kind)
  : fieldName_(Move(fieldName))
{
    pod.kind_ = kind;
    pod.index_ = 0;
}

uint32_t
Export::funcIndex() const
{
    MOZ_ASSERT(pod.kind_ == DefinitionKind::Function);
    return pod.index_;
}

uint32_t
Export::globalIndex() const
{
    MOZ_ASSERT(pod.kind_ == DefinitionKind::Global);
    return pod.index_;
}

size_t
Export::serializedSize() const
{
    return fieldName_.serializedSize() +
           sizeof(pod);
}

uint8_t*
Export::serialize(uint8_t* cursor) const
{
    cursor = fieldName_.serialize(cursor);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t*
Export::deserialize(const uint8_t* cursor)
{
    (cursor = fieldName_.deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

size_t
Export::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return fieldName_.sizeOfExcludingThis(mallocSizeOf);
}

size_t
ElemSegment::serializedSize() const
{
    return sizeof(tableIndex) +
           sizeof(offset) +
           SerializedPodVectorSize(elemFuncIndices) +
           SerializedPodVectorSize(elemCodeRangeIndices);
}

uint8_t*
ElemSegment::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &tableIndex, sizeof(tableIndex));
    cursor = WriteBytes(cursor, &offset, sizeof(offset));
    cursor = SerializePodVector(cursor, elemFuncIndices);
    cursor = SerializePodVector(cursor, elemCodeRangeIndices);
    return cursor;
}

const uint8_t*
ElemSegment::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &tableIndex, sizeof(tableIndex))) &&
    (cursor = ReadBytes(cursor, &offset, sizeof(offset))) &&
    (cursor = DeserializePodVector(cursor, &elemFuncIndices)) &&
    (cursor = DeserializePodVector(cursor, &elemCodeRangeIndices));
    return cursor;
}

size_t
ElemSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return elemFuncIndices.sizeOfExcludingThis(mallocSizeOf) +
           elemCodeRangeIndices.sizeOfExcludingThis(mallocSizeOf);
}

size_t
Module::serializedSize() const
{
    return SerializedPodVectorSize(code_) +
           linkData_.serializedSize() +
           SerializedVectorSize(imports_) +
           SerializedVectorSize(exports_) +
           SerializedPodVectorSize(dataSegments_) +
           SerializedVectorSize(elemSegments_) +
           metadata_->serializedSize() +
           SerializedPodVectorSize(bytecode_->bytes);
}

uint8_t*
Module::serialize(uint8_t* cursor) const
{
    cursor = SerializePodVector(cursor, code_);
    cursor = linkData_.serialize(cursor);
    cursor = SerializeVector(cursor, imports_);
    cursor = SerializeVector(cursor, exports_);
    cursor = SerializePodVector(cursor, dataSegments_);
    cursor = SerializeVector(cursor, elemSegments_);
    cursor = metadata_->serialize(cursor);
    cursor = SerializePodVector(cursor, bytecode_->bytes);
    return cursor;
}

/* static */ const uint8_t*
Module::deserialize(const uint8_t* cursor, SharedModule* module, Metadata* maybeMetadata)
{
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
    MOZ_RELEASE_ASSERT(!!maybeMetadata == metadata->isAsmJS());

    MutableBytes bytecode = js_new<ShareableBytes>();
    if (!bytecode)
        return nullptr;
    cursor = DeserializePodVector(cursor, &bytecode->bytes);
    if (!cursor)
        return nullptr;

    *module = js_new<Module>(Move(code),
                             Move(linkData),
                             Move(imports),
                             Move(exports),
                             Move(dataSegments),
                             Move(elemSegments),
                             *metadata,
                             *bytecode);
    if (!*module)
        return nullptr;

    return cursor;
}

/* virtual */ void
Module::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                      Metadata::SeenSet* seenMetadata,
                      ShareableBytes::SeenSet* seenBytes,
                      size_t* code,
                      size_t* data) const
{
    *data += mallocSizeOf(this) +
             code_.sizeOfExcludingThis(mallocSizeOf) +
             linkData_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(imports_, mallocSizeOf) +
             SizeOfVectorExcludingThis(exports_, mallocSizeOf) +
             dataSegments_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(elemSegments_, mallocSizeOf) +
             metadata_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenMetadata) +
             bytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
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
        uint32_t tableLength = tables[seg.tableIndex]->length();
        uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);

        if (offset > tableLength || tableLength - offset < seg.elemCodeRangeIndices.length()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT, "elem", "table");
            return false;
        }
    }

    if (memoryObj) {
        for (const DataSegment& seg : dataSegments_) {
            uint32_t memoryLength = memoryObj->buffer().byteLength();
            uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);

            if (offset > memoryLength || memoryLength - offset < seg.length) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT, "data", "memory");
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
            uint32_t elemFuncIndex = seg.elemFuncIndices[i];
            if (elemFuncIndex < funcImports.length()) {
                MOZ_ASSERT(!metadata().isAsmJS());
                MOZ_ASSERT(!table.isTypedFunction());
                MOZ_ASSERT(seg.elemCodeRangeIndices[i] == UINT32_MAX);

                HandleFunction f = funcImports[elemFuncIndex];
                if (!IsExportedWasmFunction(f)) {
                    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_TABLE_VALUE);
                    return false;
                }

                WasmInstanceObject* exportInstanceObj = ExportedFunctionToInstanceObject(f);
                const CodeRange& cr = exportInstanceObj->getExportedFunctionCodeRange(f);
                Instance& exportInstance = exportInstanceObj->instance();
                table.set(offset + i, exportInstance.codeBase() + cr.funcTableEntry(), exportInstance);
            } else {
                MOZ_ASSERT(seg.elemCodeRangeIndices[i] != UINT32_MAX);

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

        uint32_t funcDefIndex = ExportedFunctionToDefinitionIndex(f);
        Instance& instance = ExportedFunctionToInstance(f);
        const FuncDefExport& funcDefExport = instance.metadata().lookupFuncDefExport(funcDefIndex);

        if (funcDefExport.sig() != metadata_->funcImports[i].sig()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_SIG);
            return false;
        }
    }

    return true;
}

static bool
CheckResizableLimits(JSContext* cx, uint32_t declaredMin, Maybe<uint32_t> declaredMax,
                     uint32_t actualLength, Maybe<uint32_t> actualMax,
                     bool isAsmJS, const char* kind)
{
    if (isAsmJS) {
        MOZ_ASSERT(actualLength >= declaredMin);
        MOZ_ASSERT(!declaredMax);
        MOZ_ASSERT(actualLength == actualMax.value());
        return true;
    }

    if (actualLength < declaredMin || actualLength > declaredMax.valueOr(UINT32_MAX)) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_SIZE, kind);
        return false;
    }

    if ((actualMax && (!declaredMax || *actualMax > *declaredMax)) || (!actualMax && declaredMax)) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_MAX, kind);
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

        if (!CheckResizableLimits(cx, declaredMin, declaredMax,
                                  buffer.byteLength(), buffer.wasmMaxSize(),
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

        RootedObject proto(cx);
        if (metadata_->assumptions.newFormat)
            proto = &cx->global()->getPrototype(JSProto_WasmMemory).toObject();

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
        if (!CheckResizableLimits(cx, td.limits.initial, td.limits.maximum,
                                  table.length(), table.maximum(),
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
    if (exp.funcIndex() < funcImports.length()) {
        val.setObject(*funcImports[exp.funcIndex()]);
        return true;
    }

    uint32_t funcDefIndex = exp.funcIndex() - funcImports.length();

    RootedFunction fun(cx);
    if (!instanceObj->getExportedFunction(cx, instanceObj, funcDefIndex, &fun))
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
        float f = val.f32().fp();
        if (JitOptions.wasmTestMode && IsNaN(f)) {
            uint32_t bits = val.f32().bits();
            RootedObject obj(cx, CreateCustomNaNObject(cx, (float*)&bits));
            if (!obj)
                return false;
            jsval.set(ObjectValue(*obj));
            return true;
        }
        jsval.set(DoubleValue(double(f)));
        return true;
      }
      case ValType::F64: {
        double d = val.f64().fp();
        if (JitOptions.wasmTestMode && IsNaN(d)) {
            uint64_t bits = val.f64().bits();
            RootedObject obj(cx, CreateCustomNaNObject(cx, (double*)&bits));
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

    exportObj.set(JS_NewPlainObject(cx));
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
            if (metadata.assumptions.newFormat)
                val = ObjectValue(*memoryObj);
            else
                val = ObjectValue(memoryObj->buffer());
            break;
          case DefinitionKind::Global:
            if (!GetGlobalExport(cx, metadata.globals, exp.globalIndex(), globalImports, &val))
                return false;
            break;
        }

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
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

    if (metadata_->hasStartFunction()) {
        uint32_t startFuncIndex = metadata_->startFuncIndex();
        FixedInvokeArgs<0> args(cx);
        if (startFuncIndex < funcImports.length()) {
            RootedValue fval(cx, ObjectValue(*funcImports[startFuncIndex]));
            RootedValue thisv(cx);
            RootedValue rval(cx);
            if (!Call(cx, fval, thisv, args, &rval))
                return false;
        } else {
            uint32_t funcDefIndex = startFuncIndex - funcImports.length();
            if (!instance->instance().callExport(cx, funcDefIndex, args))
                return false;
        }
    }

    uint32_t mode = uint32_t(metadata().isAsmJS() ? Telemetry::ASMJS : Telemetry::WASM);
    cx->runtime()->addTelemetry(JS_TELEMETRY_AOT_USAGE, mode);

    return true;
}
