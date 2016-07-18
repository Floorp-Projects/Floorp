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

#include "vm/ArrayBufferObject-inl.h"
#include "vm/Debugger-inl.h"

using namespace js;
using namespace js::wasm;

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

Export::Export(UniqueChars fieldName, uint32_t funcExportIndex)
  : fieldName_(Move(fieldName))
{
    pod.kind_ = DefinitionKind::Function;
    pod.funcExportIndex_ = funcExportIndex;
}

Export::Export(UniqueChars fieldName, DefinitionKind kind)
  : fieldName_(Move(fieldName))
{
    pod.kind_ = kind;
    pod.funcExportIndex_ = 0;
}

uint32_t
Export::funcExportIndex() const
{
    MOZ_ASSERT(pod.kind_ == DefinitionKind::Function);
    return pod.funcExportIndex_;
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
           SerializedPodVectorSize(elems);
}

uint8_t*
ElemSegment::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &tableIndex, sizeof(tableIndex));
    cursor = SerializePodVector(cursor, elems);
    return cursor;
}

const uint8_t*
ElemSegment::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &tableIndex, sizeof(tableIndex))) &&
    (cursor = DeserializePodVector(cursor, &elems));
    return cursor;
}

size_t
ElemSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return elems.sizeOfExcludingThis(mallocSizeOf);
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

    RootedArrayBufferObjectMaybeShared buffer(cx);
    if (memory) {
        buffer = &memory->buffer();
        uint32_t length = buffer->byteLength();
        if (length < metadata_->minMemoryLength || length > metadata_->maxMemoryLength) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_MEM_IMP_SIZE);
            return false;
        }

        // This can't happen except via the shell toggling signals.enabled.
        if (metadata_->assumptions.usesSignal.forOOB &&
            !buffer->is<SharedArrayBufferObject>() &&
            !buffer->as<ArrayBufferObject>().isWasmMapped())
        {
            JS_ReportError(cx, "can't access same buffer with and without signals enabled");
            return false;
        }
    } else {
        buffer = ArrayBufferObject::createForWasm(cx, metadata_->minMemoryLength,
                                                  metadata_->assumptions.usesSignal.forOOB);
        if (!buffer)
            return false;

        RootedObject proto(cx);
        if (metadata_->assumptions.newFormat)
            proto = &cx->global()->getPrototype(JSProto_WasmMemory).toObject();

        memory.set(WasmMemoryObject::create(cx, buffer, proto));
        if (!memory)
            return false;
    }

    MOZ_ASSERT(buffer->is<SharedArrayBufferObject>() || buffer->as<ArrayBufferObject>().isWasm());

    uint8_t* memoryBase = memory->buffer().dataPointerEither().unwrap(/* memcpy */);
    for (const DataSegment& seg : dataSegments_)
        memcpy(memoryBase + seg.memoryOffset, bytecode_->begin() + seg.bytecodeOffset, seg.length);

    return true;
}

bool
Module::instantiateTable(JSContext* cx, const CodeSegment& cs, SharedTableVector* tables) const
{
    for (const TableDesc& tableDesc : metadata_->tables) {
        SharedTable table = Table::create(cx, tableDesc.kind, tableDesc.length);
        if (!table || !tables->emplaceBack(table))
            return false;

        for (size_t i = 0; i < table->length(); i++)
            table->array()[i] = cs.badIndirectCallCode();
    }

    for (const ElemSegment& seg : elemSegments_) {
        SharedTable& table = (*tables)[seg.tableIndex];
        MOZ_ASSERT(seg.offset + seg.elems.length() <= table->length());
        for (size_t i = 0; i < seg.elems.length(); i++)
            table->array()[seg.offset + i] = cs.code() + seg.elems[i];
    }

    return true;
}

static bool
CreateExportObject(JSContext* cx,
                   HandleWasmInstanceObject instanceObj,
                   HandleWasmMemoryObject memoryObj,
                   const ExportVector& exports,
                   MutableHandleObject exportObj)
{
    const Instance& instance = instanceObj->instance();
    const Metadata& metadata = instance.metadata();
    const SharedTableVector& tables = instance.tables();

    if (metadata.isAsmJS() && exports.length() == 1 && strlen(exports[0].fieldName()) == 0) {
        RootedFunction fun(cx);
        if (!instanceObj->getExportedFunction(cx, instanceObj, exports[0].funcExportIndex(), &fun))
            return false;
        exportObj.set(fun);
        return true;
    }

    exportObj.set(JS_NewPlainObject(cx));
    if (!exportObj)
        return false;

    RootedWasmTableObject tableObj(cx);
    for (const Export& exp : exports) {
        JSAtom* atom = AtomizeUTF8Chars(cx, exp.fieldName(), strlen(exp.fieldName()));
        if (!atom)
            return false;

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        switch (exp.kind()) {
          case DefinitionKind::Function: {
            RootedFunction fun(cx);
            if (!instanceObj->getExportedFunction(cx, instanceObj, exp.funcExportIndex(), &fun))
                return false;
            val = ObjectValue(*fun);
            break;
          }
          case DefinitionKind::Table: {
            MOZ_ASSERT(tables.length() == 1);
            if (!tableObj) {
                tableObj = WasmTableObject::create(cx, *tables[0]);
                if (!tableObj)
                    return false;
            }
            val = ObjectValue(*tableObj);
            break;
          }
          case DefinitionKind::Memory: {
            if (metadata.assumptions.newFormat)
                val = ObjectValue(*memoryObj);
            else
                val = ObjectValue(memoryObj->buffer());
            break;
          }
        }

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
            return false;
    }

    return true;
}

bool
Module::instantiate(JSContext* cx,
                    Handle<FunctionVector> funcImports,
                    HandleWasmMemoryObject memImport,
                    HandleObject instanceProto,
                    MutableHandleWasmInstanceObject instanceObj) const
{
    MOZ_ASSERT(funcImports.length() == metadata_->funcImports.length());

    RootedWasmMemoryObject memory(cx, memImport);
    if (!instantiateMemory(cx, &memory))
        return false;

    auto cs = CodeSegment::create(cx, code_, linkData_, *metadata_, memory);
    if (!cs)
        return false;

    SharedTableVector tables;
    if (!instantiateTable(cx, *cs, &tables))
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

    // Create the Instance, ensuring that it is traceable via 'instanceObj'
    // before any GC can occur and invalidate the pointers stored in global
    // memory.

    {
        instanceObj.set(WasmInstanceObject::create(cx, instanceProto));
        if (!instanceObj)
            return false;

        auto instance = cx->make_unique<Instance>(Move(cs),
                                                  *metadata_,
                                                  maybeBytecode,
                                                  memory,
                                                  Move(tables),
                                                  funcImports);
        if (!instance)
            return false;

        instanceObj->init(Move(instance));
    }

    // Create the export object.

    RootedObject exportObj(cx);
    if (!CreateExportObject(cx, instanceObj, memory, exports_, &exportObj))
        return false;

    JSAtom* atom = Atomize(cx, InstanceExportField, strlen(InstanceExportField));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));

    RootedValue val(cx, ObjectValue(*exportObj));
    if (!JS_DefinePropertyById(cx, instanceObj, id, val, JSPROP_ENUMERATE))
        return false;

    // Done! Notify the Debugger of the new Instance.

    Debugger::onNewWasmInstance(cx, instanceObj);

    // Call the start function, if there's one. By specification, it does not
    // take any arguments nor does it return a value, so just create a dummy
    // arguments object.

    if (metadata_->hasStartFunction()) {
        FixedInvokeArgs<0> args(cx);
        if (!instanceObj->instance().callExport(cx, metadata_->startFuncExportIndex(), args))
            return false;
    }

    return true;
}
