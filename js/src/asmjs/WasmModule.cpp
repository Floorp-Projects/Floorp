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

#include "mozilla/Atomics.h"

#include "asmjs/WasmInstance.h"
#include "asmjs/WasmJS.h"
#include "asmjs/WasmSerialize.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/Debugger-inl.h"

using namespace js;
using namespace js::wasm;

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

size_t
ExportMap::serializedSize() const
{
    return SerializedVectorSize(fieldNames) +
           SerializedPodVectorSize(fieldsToExports);
}

uint8_t*
ExportMap::serialize(uint8_t* cursor) const
{
    cursor = SerializeVector(cursor, fieldNames);
    cursor = SerializePodVector(cursor, fieldsToExports);
    return cursor;
}

const uint8_t*
ExportMap::deserialize(const uint8_t* cursor)
{
    (cursor = DeserializeVector(cursor, &fieldNames)) &&
    (cursor = DeserializePodVector(cursor, &fieldsToExports));
    return cursor;
}

size_t
ExportMap::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return SizeOfVectorExcludingThis(fieldNames, mallocSizeOf) &&
           fieldsToExports.sizeOfExcludingThis(mallocSizeOf);
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
           exportMap_.serializedSize() +
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
    cursor = exportMap_.serialize(cursor);
    cursor = SerializePodVector(cursor, dataSegments_);
    cursor = SerializeVector(cursor, elemSegments_);
    cursor = metadata_->serialize(cursor);
    cursor = SerializePodVector(cursor, bytecode_->bytes);
    return cursor;
}

/* static */ const uint8_t*
Module::deserialize(const uint8_t* cursor, UniquePtr<Module>* module, Metadata* maybeMetadata)
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

    ExportMap exportMap;
    cursor = exportMap.deserialize(cursor);
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

    *module = js::MakeUnique<Module>(Move(code),
                                     Move(linkData),
                                     Move(imports),
                                     Move(exportMap),
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
             exportMap_.sizeOfExcludingThis(mallocSizeOf) +
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
WasmCall(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction callee(cx, &args.callee().as<JSFunction>());

    Instance& instance = ExportedFunctionToInstance(callee);
    uint32_t exportIndex = ExportedFunctionToExportIndex(callee);

    return instance.callExport(cx, exportIndex, args);
}

static JSFunction*
NewExportedFunction(JSContext* cx, Handle<WasmInstanceObject*> instanceObj, uint32_t exportIndex)
{
    Instance& instance = instanceObj->instance();
    const Metadata& metadata = instance.metadata();
    const Export& exp = metadata.exports[exportIndex];
    unsigned numArgs = exp.sig().args().length();

    RootedAtom name(cx, instance.getFuncAtom(cx, exp.funcIndex()));
    if (!name)
        return nullptr;

    JSFunction* fun = NewNativeConstructor(cx, WasmCall, numArgs, name,
                                           gc::AllocKind::FUNCTION_EXTENDED, GenericObject,
                                           JSFunction::ASMJS_CTOR);
    if (!fun)
        return nullptr;

    fun->setExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT, ObjectValue(*instanceObj));
    fun->setExtendedSlot(FunctionExtended::WASM_EXPORT_INDEX_SLOT, Int32Value(exportIndex));
    return fun;
}

static bool
CreateExportObject(JSContext* cx,
                   HandleWasmInstanceObject instanceObj,
                   HandleWasmMemoryObject memoryObj,
                   const ExportMap& exportMap,
                   const Metadata& metadata,
                   MutableHandleObject exportObj)
{
    MOZ_ASSERT(exportMap.fieldNames.length() == exportMap.fieldsToExports.length());

    if (metadata.isAsmJS() &&
        exportMap.fieldNames.length() == 1 &&
        strlen(exportMap.fieldNames[0].get()) == 0)
    {
        exportObj.set(NewExportedFunction(cx, instanceObj, 0));
        return !!exportObj;
    }

    exportObj.set(JS_NewPlainObject(cx));
    if (!exportObj)
        return false;

    Rooted<ValueVector> vals(cx, ValueVector(cx));
    for (size_t exportIndex = 0; exportIndex < metadata.exports.length(); exportIndex++) {
        JSFunction* fun = NewExportedFunction(cx, instanceObj, exportIndex);
        if (!fun || !vals.append(ObjectValue(*fun)))
            return false;
    }

    for (size_t fieldIndex = 0; fieldIndex < exportMap.fieldNames.length(); fieldIndex++) {
        const char* fieldName = exportMap.fieldNames[fieldIndex].get();
        JSAtom* atom = AtomizeUTF8Chars(cx, fieldName, strlen(fieldName));
        if (!atom)
            return false;

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        uint32_t exportIndex = exportMap.fieldsToExports[fieldIndex];
        if (exportIndex == MemoryExport) {
            if (metadata.assumptions.newFormat)
                val = ObjectValue(*memoryObj);
            else
                val = ObjectValue(memoryObj->buffer());
        } else {
            val = vals[exportIndex];
        }

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
            return false;
    }

    return true;
}

static const char ExportField[] = "exports";

bool
Module::instantiate(JSContext* cx,
                    Handle<FunctionVector> funcImports,
                    HandleWasmMemoryObject memImport,
                    HandleWasmInstanceObject instanceObj) const
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
    if (!CreateExportObject(cx, instanceObj, memory, exportMap_, *metadata_, &exportObj))
        return false;

    instanceObj->initExportsObject(exportObj);

    JSAtom* atom = Atomize(cx, ExportField, strlen(ExportField));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));

    RootedValue val(cx, ObjectValue(*exportObj));
    if (!JS_DefinePropertyById(cx, instanceObj, id, val, JSPROP_ENUMERATE))
        return false;

    // Done! Notify the Debugger of the new Instance.

    Debugger::onNewWasmInstance(cx, instanceObj);

    return true;
}

bool
wasm::IsExportedFunction(JSFunction* fun)
{
    return fun->maybeNative() == WasmCall;
}

Instance&
wasm::ExportedFunctionToInstance(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_INSTANCE_SLOT);
    return v.toObject().as<WasmInstanceObject>().instance();
}

uint32_t
wasm::ExportedFunctionToExportIndex(JSFunction* fun)
{
    MOZ_ASSERT(IsExportedFunction(fun));
    const Value& v = fun->getExtendedSlot(FunctionExtended::WASM_EXPORT_INDEX_SLOT);
    return v.toInt32();
}
