/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
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

#include "asmjs/AsmJSModule.h"

#include "mozilla/Compression.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/PodOperations.h"

#include "jsprf.h"

#include "asmjs/WasmSerialize.h"
#include "frontend/Parser.h"
#include "js/Class.h"
#include "js/MemoryMetrics.h"

#include "jsobjinlines.h"

#include "frontend/ParseNode-inl.h"

using namespace js;
using namespace js::frontend;
using namespace js::jit;
using namespace js::wasm;
using mozilla::PodZero;
using mozilla::PodEqual;
using mozilla::Compression::LZ4;

AsmJSModule::AsmJSModule(ScriptSource* scriptSource, uint32_t srcStart, uint32_t srcBodyStart,
                         bool strict)
  : scriptSource_(scriptSource),
    srcStart_(srcStart),
    srcBodyStart_(srcBodyStart),
    globalArgumentName_(nullptr),
    importArgumentName_(nullptr),
    bufferArgumentName_(nullptr)
{
    mozilla::PodZero(&pod);
    pod.minHeapLength_ = RoundUpToNextValidAsmJSHeapLength(0);
    pod.maxHeapLength_ = 0x80000000;
    pod.strict_ = strict;

    // AsmJSCheckedImmediateRange should be defined to be at most the minimum
    // heap length so that offsets can be folded into bounds checks.
    MOZ_ASSERT(pod.minHeapLength_ - AsmJSCheckedImmediateRange <= pod.minHeapLength_);
}

void
AsmJSModule::trace(JSTracer* trc)
{
    if (wasm_)
        wasm_->trace(trc);
    for (Global& global : globals_)
        global.trace(trc);
    for (Export& exp : exports_)
        exp.trace(trc);
    if (globalArgumentName_)
        TraceManuallyBarrieredEdge(trc, &globalArgumentName_, "asm.js global argument name");
    if (importArgumentName_)
        TraceManuallyBarrieredEdge(trc, &importArgumentName_, "asm.js import argument name");
    if (bufferArgumentName_)
        TraceManuallyBarrieredEdge(trc, &bufferArgumentName_, "asm.js buffer argument name");
}

void
AsmJSModule::addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* asmJSModuleCode,
                           size_t* asmJSModuleData)
{
    if (wasm_)
        wasm_->addSizeOfMisc(mallocSizeOf, asmJSModuleCode, asmJSModuleData);

    if (linkData_)
        *asmJSModuleData += linkData_->sizeOfExcludingThis(mallocSizeOf);

    *asmJSModuleData += mallocSizeOf(this) +
                        globals_.sizeOfExcludingThis(mallocSizeOf) +
                        imports_.sizeOfExcludingThis(mallocSizeOf) +
                        exports_.sizeOfExcludingThis(mallocSizeOf);
}

void
AsmJSModule::finish(Module* wasm, wasm::UniqueStaticLinkData linkData,
                    uint32_t endBeforeCurly, uint32_t endAfterCurly)
{
    MOZ_ASSERT(!isFinished());

    wasm_.reset(wasm);
    linkData_ = Move(linkData);

    MOZ_ASSERT(endBeforeCurly >= srcBodyStart_);
    MOZ_ASSERT(endAfterCurly >= srcBodyStart_);
    pod.srcLength_ = endBeforeCurly - srcStart_;
    pod.srcLengthWithRightBrace_ = endAfterCurly - srcStart_;

    MOZ_ASSERT(isFinished());
}

bool
js::OnDetachAsmJSArrayBuffer(JSContext* cx, Handle<ArrayBufferObject*> buffer)
{
    for (Module* m = cx->runtime()->linkedWasmModules; m; m = m->nextLinked()) {
        if (buffer == m->maybeBuffer() && !m->detachHeap(cx))
            return false;
    }
    return true;
}

static void
AsmJSModuleObject_finalize(FreeOp* fop, JSObject* obj)
{
    AsmJSModuleObject& moduleObj = obj->as<AsmJSModuleObject>();
    if (moduleObj.hasModule())
        fop->delete_(&moduleObj.module());
}

static void
AsmJSModuleObject_trace(JSTracer* trc, JSObject* obj)
{
    AsmJSModuleObject& moduleObj = obj->as<AsmJSModuleObject>();
    if (moduleObj.hasModule())
        moduleObj.module().trace(trc);
}

const Class AsmJSModuleObject::class_ = {
    "AsmJSModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_DELAY_METADATA_CALLBACK |
    JSCLASS_HAS_RESERVED_SLOTS(AsmJSModuleObject::RESERVED_SLOTS),
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    AsmJSModuleObject_finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    AsmJSModuleObject_trace
};

AsmJSModuleObject*
AsmJSModuleObject::create(ExclusiveContext* cx)
{
    AutoSetNewObjectMetadata metadata(cx);
    JSObject* obj = NewObjectWithGivenProto(cx, &AsmJSModuleObject::class_, nullptr);
    if (!obj)
        return nullptr;
    return &obj->as<AsmJSModuleObject>();
}

bool
AsmJSModuleObject::hasModule() const
{
    MOZ_ASSERT(is<AsmJSModuleObject>());
    return !getReservedSlot(MODULE_SLOT).isUndefined();
}

void
AsmJSModuleObject::setModule(AsmJSModule* newModule)
{
    MOZ_ASSERT(is<AsmJSModuleObject>());
    if (hasModule())
        js_delete(&module());
    setReservedSlot(MODULE_SLOT, PrivateValue(newModule));
}

AsmJSModule&
AsmJSModuleObject::module() const
{
    MOZ_ASSERT(is<AsmJSModuleObject>());
    return *(AsmJSModule*)getReservedSlot(MODULE_SLOT).toPrivate();
}

uint8_t*
AsmJSModule::Global::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    cursor = SerializeName(cursor, name_);
    return cursor;
}

size_t
AsmJSModule::Global::serializedSize() const
{
    return sizeof(pod) +
           SerializedNameSize(name_);
}

const uint8_t*
AsmJSModule::Global::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod, sizeof(pod))) &&
    (cursor = DeserializeName(cx, cursor, &name_));
    return cursor;
}

bool
AsmJSModule::Global::clone(JSContext* cx, Global* out) const
{
    *out = *this;
    return true;
}

uint8_t*
AsmJSModule::Export::serialize(uint8_t* cursor) const
{
    cursor = SerializeName(cursor, name_);
    cursor = SerializeName(cursor, maybeFieldName_);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

size_t
AsmJSModule::Export::serializedSize() const
{
    return SerializedNameSize(name_) +
           SerializedNameSize(maybeFieldName_) +
           sizeof(pod);
}

const uint8_t*
AsmJSModule::Export::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    (cursor = DeserializeName(cx, cursor, &name_)) &&
    (cursor = DeserializeName(cx, cursor, &maybeFieldName_)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

bool
AsmJSModule::Export::clone(JSContext* cx, Export* out) const
{
    out->name_ = name_;
    out->maybeFieldName_ = maybeFieldName_;
    out->pod = pod;
    return true;
}

size_t
AsmJSModule::serializedSize() const
{
    MOZ_ASSERT(isFinished());
    return wasm_->serializedSize() +
           linkData_->serializedSize() +
           sizeof(pod) +
           SerializedVectorSize(globals_) +
           SerializedPodVectorSize(imports_) +
           SerializedVectorSize(exports_) +
           SerializedNameSize(globalArgumentName_) +
           SerializedNameSize(importArgumentName_) +
           SerializedNameSize(bufferArgumentName_);
}

uint8_t*
AsmJSModule::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(isFinished());
    cursor = wasm_->serialize(cursor);
    cursor = linkData_->serialize(cursor);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    cursor = SerializeVector(cursor, globals_);
    cursor = SerializePodVector(cursor, imports_);
    cursor = SerializeVector(cursor, exports_);
    cursor = SerializeName(cursor, globalArgumentName_);
    cursor = SerializeName(cursor, importArgumentName_);
    cursor = SerializeName(cursor, bufferArgumentName_);
    return cursor;
}

const uint8_t*
AsmJSModule::deserialize(ExclusiveContext* cx, const uint8_t* cursor)
{
    linkData_ = cx->make_unique<StaticLinkData>();
    if (!linkData_)
        return nullptr;

    // To avoid GC-during-deserialization corner cases, prevent atoms from
    // being collected.
    AutoKeepAtoms aka(cx->perThreadData);

    (cursor = Module::deserialize(cx, cursor, &wasm_)) &&
    (cursor = linkData_->deserialize(cx, cursor)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod))) &&
    (cursor = DeserializeVector(cx, cursor, &globals_)) &&
    (cursor = DeserializePodVector(cx, cursor, &imports_)) &&
    (cursor = DeserializeVector(cx, cursor, &exports_)) &&
    (cursor = DeserializeName(cx, cursor, &globalArgumentName_)) &&
    (cursor = DeserializeName(cx, cursor, &importArgumentName_)) &&
    (cursor = DeserializeName(cx, cursor, &bufferArgumentName_));

    return cursor;
}

bool
AsmJSModule::clone(JSContext* cx, HandleAsmJSModule obj) const
{
    auto out = cx->new_<AsmJSModule>(scriptSource(), srcStart_, srcBodyStart_, pod.strict_);
    if (!out)
        return false;

    obj->setModule(out);

    out->wasm_ = wasm_->clone(cx, *linkData_);
    if (!out->wasm_)
        return false;

    out->linkData_ = cx->make_unique<StaticLinkData>();
    if (!out->linkData_ || !linkData_->clone(cx, out->linkData_.get()))
        return false;

    out->pod = pod;

    if (!CloneVector(cx, globals_, &out->globals_) ||
        !ClonePodVector(cx, imports_, &out->imports_) ||
        !CloneVector(cx, exports_, &out->exports_))
    {
        return false;
    }

    out->globalArgumentName_ = globalArgumentName_;
    out->importArgumentName_ = importArgumentName_;
    out->bufferArgumentName_ = bufferArgumentName_;
    return true;
}

struct PropertyNameWrapper
{
    PropertyName* name;

    PropertyNameWrapper()
      : name(nullptr)
    {}
    explicit PropertyNameWrapper(PropertyName* name)
      : name(name)
    {}
    size_t serializedSize() const {
        return SerializedNameSize(name);
    }
    uint8_t* serialize(uint8_t* cursor) const {
        return SerializeName(cursor, name);
    }
    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor) {
        return DeserializeName(cx, cursor, &name);
    }
};

class ModuleChars
{
  protected:
    uint32_t isFunCtor_;
    Vector<PropertyNameWrapper, 0, SystemAllocPolicy> funCtorArgs_;

  public:
    static uint32_t beginOffset(AsmJSParser& parser) {
        return parser.pc->maybeFunction->pn_pos.begin;
    }

    static uint32_t endOffset(AsmJSParser& parser) {
        TokenPos pos(0, 0);  // initialize to silence GCC warning
        MOZ_ALWAYS_TRUE(parser.tokenStream.peekTokenPos(&pos, TokenStream::Operand));
        return pos.end;
    }
};

class ModuleCharsForStore : ModuleChars
{
    uint32_t uncompressedSize_;
    uint32_t compressedSize_;
    Vector<char, 0, SystemAllocPolicy> compressedBuffer_;

  public:
    bool init(AsmJSParser& parser) {
        MOZ_ASSERT(beginOffset(parser) < endOffset(parser));

        uncompressedSize_ = (endOffset(parser) - beginOffset(parser)) * sizeof(char16_t);
        size_t maxCompressedSize = LZ4::maxCompressedSize(uncompressedSize_);
        if (maxCompressedSize < uncompressedSize_)
            return false;

        if (!compressedBuffer_.resize(maxCompressedSize))
            return false;

        const char16_t* chars = parser.tokenStream.rawCharPtrAt(beginOffset(parser));
        const char* source = reinterpret_cast<const char*>(chars);
        size_t compressedSize = LZ4::compress(source, uncompressedSize_, compressedBuffer_.begin());
        if (!compressedSize || compressedSize > UINT32_MAX)
            return false;

        compressedSize_ = compressedSize;

        // For a function statement or named function expression:
        //   function f(x,y,z) { abc }
        // the range [beginOffset, endOffset) captures the source:
        //   f(x,y,z) { abc }
        // An unnamed function expression captures the same thing, sans 'f'.
        // Since asm.js modules do not contain any free variables, equality of
        // [beginOffset, endOffset) is sufficient to guarantee identical code
        // generation, modulo MachineId.
        //
        // For functions created with 'new Function', function arguments are
        // not present in the source so we must manually explicitly serialize
        // and match the formals as a Vector of PropertyName.
        isFunCtor_ = parser.pc->isFunctionConstructorBody();
        if (isFunCtor_) {
            unsigned numArgs;
            ParseNode* arg = FunctionArgsList(parser.pc->maybeFunction, &numArgs);
            for (unsigned i = 0; i < numArgs; i++, arg = arg->pn_next) {
                if (!funCtorArgs_.append(arg->name()))
                    return false;
            }
        }

        return true;
    }

    size_t serializedSize() const {
        return sizeof(uint32_t) +
               sizeof(uint32_t) +
               compressedSize_ +
               sizeof(uint32_t) +
               (isFunCtor_ ? SerializedVectorSize(funCtorArgs_) : 0);
    }

    uint8_t* serialize(uint8_t* cursor) const {
        cursor = WriteScalar<uint32_t>(cursor, uncompressedSize_);
        cursor = WriteScalar<uint32_t>(cursor, compressedSize_);
        cursor = WriteBytes(cursor, compressedBuffer_.begin(), compressedSize_);
        cursor = WriteScalar<uint32_t>(cursor, isFunCtor_);
        if (isFunCtor_)
            cursor = SerializeVector(cursor, funCtorArgs_);
        return cursor;
    }
};

class ModuleCharsForLookup : ModuleChars
{
    Vector<char16_t, 0, SystemAllocPolicy> chars_;

  public:
    const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor) {
        uint32_t uncompressedSize;
        cursor = ReadScalar<uint32_t>(cursor, &uncompressedSize);

        uint32_t compressedSize;
        cursor = ReadScalar<uint32_t>(cursor, &compressedSize);

        if (!chars_.resize(uncompressedSize / sizeof(char16_t)))
            return nullptr;

        const char* source = reinterpret_cast<const char*>(cursor);
        char* dest = reinterpret_cast<char*>(chars_.begin());
        if (!LZ4::decompress(source, dest, uncompressedSize))
            return nullptr;

        cursor += compressedSize;

        cursor = ReadScalar<uint32_t>(cursor, &isFunCtor_);
        if (isFunCtor_)
            cursor = DeserializeVector(cx, cursor, &funCtorArgs_);

        return cursor;
    }

    bool match(AsmJSParser& parser) const {
        const char16_t* parseBegin = parser.tokenStream.rawCharPtrAt(beginOffset(parser));
        const char16_t* parseLimit = parser.tokenStream.rawLimit();
        MOZ_ASSERT(parseLimit >= parseBegin);
        if (uint32_t(parseLimit - parseBegin) < chars_.length())
            return false;
        if (!PodEqual(chars_.begin(), parseBegin, chars_.length()))
            return false;
        if (isFunCtor_ != parser.pc->isFunctionConstructorBody())
            return false;
        if (isFunCtor_) {
            // For function statements, the closing } is included as the last
            // character of the matched source. For Function constructor,
            // parsing terminates with EOF which we must explicitly check. This
            // prevents
            //   new Function('"use asm"; function f() {} return f')
            // from incorrectly matching
            //   new Function('"use asm"; function f() {} return ff')
            if (parseBegin + chars_.length() != parseLimit)
                return false;
            unsigned numArgs;
            ParseNode* arg = FunctionArgsList(parser.pc->maybeFunction, &numArgs);
            if (funCtorArgs_.length() != numArgs)
                return false;
            for (unsigned i = 0; i < funCtorArgs_.length(); i++, arg = arg->pn_next) {
                if (funCtorArgs_[i].name != arg->name())
                    return false;
            }
        }
        return true;
    }
};

JS::AsmJSCacheResult
js::StoreAsmJSModuleInCache(AsmJSParser& parser, const AsmJSModule& module, ExclusiveContext* cx)
{
    MachineId machineId;
    if (!machineId.extractCurrentState(cx))
        return JS::AsmJSCache_InternalError;

    ModuleCharsForStore moduleChars;
    if (!moduleChars.init(parser))
        return JS::AsmJSCache_InternalError;

    size_t serializedSize = machineId.serializedSize() +
                            moduleChars.serializedSize() +
                            module.serializedSize();

    JS::OpenAsmJSCacheEntryForWriteOp open = cx->asmJSCacheOps().openEntryForWrite;
    if (!open)
        return JS::AsmJSCache_Disabled_Internal;

    const char16_t* begin = parser.tokenStream.rawCharPtrAt(ModuleChars::beginOffset(parser));
    const char16_t* end = parser.tokenStream.rawCharPtrAt(ModuleChars::endOffset(parser));
    bool installed = parser.options().installedFile;

    ScopedCacheEntryOpenedForWrite entry(cx, serializedSize);
    JS::AsmJSCacheResult openResult =
        open(cx->global(), installed, begin, end, serializedSize, &entry.memory, &entry.handle);
    if (openResult != JS::AsmJSCache_Success)
        return openResult;

    uint8_t* cursor = entry.memory;
    cursor = machineId.serialize(cursor);
    cursor = moduleChars.serialize(cursor);
    cursor = module.serialize(cursor);

    MOZ_ASSERT(cursor == entry.memory + serializedSize);
    return JS::AsmJSCache_Success;
}

bool
js::LookupAsmJSModuleInCache(ExclusiveContext* cx, AsmJSParser& parser, HandleAsmJSModule moduleObj,
                             bool* loadedFromCache, UniqueChars* compilationTimeReport)
{
    int64_t usecBefore = PRMJ_Now();

    *loadedFromCache = false;

    MachineId machineId;
    if (!machineId.extractCurrentState(cx))
        return true;

    JS::OpenAsmJSCacheEntryForReadOp open = cx->asmJSCacheOps().openEntryForRead;
    if (!open)
        return true;

    const char16_t* begin = parser.tokenStream.rawCharPtrAt(ModuleChars::beginOffset(parser));
    const char16_t* limit = parser.tokenStream.rawLimit();

    ScopedCacheEntryOpenedForRead entry(cx);
    if (!open(cx->global(), begin, limit, &entry.serializedSize, &entry.memory, &entry.handle))
        return true;

    const uint8_t* cursor = entry.memory;

    MachineId cachedMachineId;
    cursor = cachedMachineId.deserialize(cx, cursor);
    if (!cursor)
        return false;
    if (machineId != cachedMachineId)
        return true;

    ModuleCharsForLookup moduleChars;
    cursor = moduleChars.deserialize(cx, cursor);
    if (!moduleChars.match(parser))
        return true;

    uint32_t srcStart = parser.pc->maybeFunction->pn_body->pn_pos.begin;
    uint32_t srcBodyStart = parser.tokenStream.currentToken().pos.end;
    bool strict = parser.pc->sc->strict() && !parser.pc->sc->hasExplicitUseStrict();

    AsmJSModule* module = cx->new_<AsmJSModule>(parser.ss, srcStart, srcBodyStart, strict);
    if (!module)
        return false;

    moduleObj->setModule(module);

    cursor = module->deserialize(cx, cursor);
    if (!cursor)
        return false;

    bool atEnd = cursor == entry.memory + entry.serializedSize;
    MOZ_ASSERT(atEnd, "Corrupt cache file");
    if (!atEnd)
        return true;

    if (module->wasm().compileArgs() != CompileArgs(cx))
        return true;

    module->staticallyLink(cx);

    if (!parser.tokenStream.advance(module->srcEndBeforeCurly()))
        return false;

    *loadedFromCache = true;

    int64_t usecAfter = PRMJ_Now();
    int ms = (usecAfter - usecBefore) / PRMJ_USEC_PER_MSEC;
    *compilationTimeReport = UniqueChars(JS_smprintf("loaded from cache in %dms", ms));
    return true;
}

