/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSModule.h"

#ifndef XP_WIN
# include <sys/mman.h>
#endif

#include "mozilla/DebugOnly.h"
#include "mozilla/PodOperations.h"

#include "jslibmath.h"
#include "jsmath.h"
#include "jsprf.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "prmjtime.h"

#include "frontend/Parser.h"
#include "jit/IonCode.h"
#include "js/MemoryMetrics.h"

#include "jsobjinlines.h"

#include "frontend/ParseNode-inl.h"

using namespace js;
using namespace jit;
using namespace frontend;
using mozilla::DebugOnly;
using mozilla::PodEqual;

void
AsmJSModule::initHeap(Handle<ArrayBufferObject*> heap, JSContext *cx)
{
    JS_ASSERT(linked_);
    JS_ASSERT(!maybeHeap_);
    maybeHeap_ = heap;
    heapDatum() = heap->dataPointer();

    JS_ASSERT(IsValidAsmJSHeapLength(heap->byteLength()));
#if defined(JS_CPU_X86)
    uint8_t *heapOffset = heap->dataPointer();
    void *heapLength = (void*)heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        const jit::AsmJSHeapAccess &access = heapAccesses_[i];
        if (access.hasLengthCheck())
            JSC::X86Assembler::setPointer(access.patchLengthAt(code_), heapLength);
        void *addr = access.patchOffsetAt(code_);
        uint32_t disp = reinterpret_cast<uint32_t>(JSC::X86Assembler::getPointer(addr));
        JS_ASSERT(disp <= INT32_MAX);
        JSC::X86Assembler::setPointer(addr, (void *)(heapOffset + disp));
    }
#elif defined(JS_CPU_ARM)
    uint32_t heapLength = heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        jit::Assembler::updateBoundsCheck(heapLength,
                                          (jit::Instruction*)(heapAccesses_[i].offset() + code_));
    }
    // We already know the exact extent of areas that need to be patched, just make sure we
    // flush all of them at once.
    jit::AutoFlushCache::updateTop(uintptr_t(code_), pod.codeBytes_);
#endif
}

static uint8_t *
AllocateExecutableMemory(ExclusiveContext *cx, size_t totalBytes)
{
    JS_ASSERT(totalBytes % AsmJSPageSize == 0);

#ifdef XP_WIN
    void *p = VirtualAlloc(nullptr, totalBytes, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!p) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }
#else  // assume Unix
    void *p = mmap(nullptr, totalBytes,
                   PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON,
                   -1, 0);
    if (p == MAP_FAILED) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }
#endif

    return (uint8_t *)p;
}

static void
DeallocateExecutableMemory(uint8_t *code, size_t totalBytes)
{
#ifdef XP_WIN
    JS_ALWAYS_TRUE(VirtualFree(code, 0, MEM_RELEASE));
#else
    JS_ALWAYS_TRUE(munmap(code, totalBytes) == 0);
#endif
}

bool
AsmJSModule::allocateAndCopyCode(ExclusiveContext *cx, MacroAssembler &masm)
{
    JS_ASSERT(!code_);

    // The global data section sits immediately after the executable (and
    // other) data allocated by the MacroAssembler, so ensure it is
    // double-aligned.
    pod.codeBytes_ = AlignBytes(masm.bytesNeeded(), sizeof(double));

    // The entire region is allocated via mmap/VirtualAlloc which requires
    // units of pages.
    pod.totalBytes_ = AlignBytes(pod.codeBytes_ + globalDataBytes(), AsmJSPageSize);

    code_ = AllocateExecutableMemory(cx, pod.totalBytes_);
    if (!code_)
        return false;

    JS_ASSERT(uintptr_t(code_) % AsmJSPageSize == 0);
    masm.executableCopy(code_);
    return true;
}

static int32_t
CoerceInPlace_ToInt32(JSContext *cx, MutableHandleValue val)
{
    int32_t i32;
    if (!ToInt32(cx, val, &i32))
        return false;
    val.set(Int32Value(i32));

    return true;
}

static int32_t
CoerceInPlace_ToNumber(JSContext *cx, MutableHandleValue val)
{
    double dbl;
    if (!ToNumber(cx, val, &dbl))
        return false;
    val.set(DoubleValue(dbl));

    return true;
}

static void
EnableActivationFromAsmJS(AsmJSActivation *activation)
{
    JSContext *cx = activation->cx();
    Activation *act = cx->mainThread().activation();
    JS_ASSERT(act->isJit());
    act->asJit()->setActive(cx);
}

static void
DisableActivationFromAsmJS(AsmJSActivation *activation)
{
    JSContext *cx = activation->cx();
    Activation *act = cx->mainThread().activation();
    JS_ASSERT(act->isJit());
    act->asJit()->setActive(cx, false);
}

namespace js {

// Defined in AsmJS.cpp:

int32_t
InvokeFromAsmJS_Ignore(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

int32_t
InvokeFromAsmJS_ToInt32(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

int32_t
InvokeFromAsmJS_ToNumber(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

}

#if defined(JS_CPU_ARM)
extern "C" {

extern int
__aeabi_idivmod(int, int);

extern int
__aeabi_uidivmod(int, int);

}
#endif

template <class F>
static inline void *
FuncCast(F *pf)
{
    return JS_FUNC_TO_DATA_PTR(void *, pf);
}

static void *
AddressOf(AsmJSImmKind kind, ExclusiveContext *cx)
{
    switch (kind) {
      case AsmJSImm_Runtime:
        return cx->runtimeAddressForJit();
      case AsmJSImm_StackLimit:
        return cx->stackLimitAddress(StackForUntrustedScript);
      case AsmJSImm_ReportOverRecursed:
        return FuncCast<void (JSContext*)>(js_ReportOverRecursed);
      case AsmJSImm_HandleExecutionInterrupt:
        return FuncCast(js_HandleExecutionInterrupt);
      case AsmJSImm_InvokeFromAsmJS_Ignore:
        return FuncCast(InvokeFromAsmJS_Ignore);
      case AsmJSImm_InvokeFromAsmJS_ToInt32:
        return FuncCast(InvokeFromAsmJS_ToInt32);
      case AsmJSImm_InvokeFromAsmJS_ToNumber:
        return FuncCast(InvokeFromAsmJS_ToNumber);
      case AsmJSImm_CoerceInPlace_ToInt32:
        return FuncCast(CoerceInPlace_ToInt32);
      case AsmJSImm_CoerceInPlace_ToNumber:
        return FuncCast(CoerceInPlace_ToNumber);
      case AsmJSImm_ToInt32:
        return FuncCast<int32_t (double)>(js::ToInt32);
      case AsmJSImm_EnableActivationFromAsmJS:
        return FuncCast(EnableActivationFromAsmJS);
      case AsmJSImm_DisableActivationFromAsmJS:
        return FuncCast(DisableActivationFromAsmJS);
#if defined(JS_CPU_ARM)
      case AsmJSImm_aeabi_idivmod:
        return FuncCast(__aeabi_idivmod);
      case AsmJSImm_aeabi_uidivmod:
        return FuncCast(__aeabi_uidivmod);
#endif
      case AsmJSImm_ModD:
        return FuncCast(NumberMod);
      case AsmJSImm_SinD:
        return FuncCast<double (double)>(sin);
      case AsmJSImm_CosD:
        return FuncCast<double (double)>(cos);
      case AsmJSImm_TanD:
        return FuncCast<double (double)>(tan);
      case AsmJSImm_ASinD:
        return FuncCast<double (double)>(asin);
      case AsmJSImm_ACosD:
        return FuncCast<double (double)>(acos);
      case AsmJSImm_ATanD:
        return FuncCast<double (double)>(atan);
      case AsmJSImm_CeilD:
        return FuncCast<double (double)>(ceil);
      case AsmJSImm_FloorD:
        return FuncCast<double (double)>(floor);
      case AsmJSImm_ExpD:
        return FuncCast<double (double)>(exp);
      case AsmJSImm_LogD:
        return FuncCast<double (double)>(log);
      case AsmJSImm_PowD:
        return FuncCast(ecmaPow);
      case AsmJSImm_ATan2D:
        return FuncCast(ecmaAtan2);
    }

    MOZ_ASSUME_UNREACHABLE("Bad AsmJSImmKind");
    return nullptr;
}

void
AsmJSModule::staticallyLink(const AsmJSStaticLinkData &linkData, ExclusiveContext *cx)
{
    // Process AsmJSStaticLinkData:

    operationCallbackExit_ = code_ + linkData.operationCallbackExitOffset;

    for (size_t i = 0; i < linkData.relativeLinks.length(); i++) {
        AsmJSStaticLinkData::RelativeLink link = linkData.relativeLinks[i];
        *(void **)(code_ + link.patchAtOffset) = code_ + link.targetOffset;
    }

    for (size_t i = 0; i < linkData.absoluteLinks.length(); i++) {
        AsmJSStaticLinkData::AbsoluteLink link = linkData.absoluteLinks[i];
        Assembler::patchDataWithValueCheck(code_ + link.patchAt.offset(),
                                           PatchedImmPtr(AddressOf(link.target, cx)),
                                           PatchedImmPtr((void*)-1));
    }

    // Initialize global data segment

    for (size_t i = 0; i < exits_.length(); i++) {
        exitIndexToGlobalDatum(i).exit = interpExitTrampoline(exits_[i]);
        exitIndexToGlobalDatum(i).fun = nullptr;
    }
}

AsmJSModule::AsmJSModule(ScriptSource *scriptSource, uint32_t charsBegin)
  : globalArgumentName_(nullptr),
    importArgumentName_(nullptr),
    bufferArgumentName_(nullptr),
    code_(nullptr),
    operationCallbackExit_(nullptr),
    linked_(false),
    loadedFromCache_(false),
    charsBegin_(charsBegin),
    scriptSource_(scriptSource)
{
    mozilla::PodZero(&pod);
    scriptSource_->incref();
    pod.minHeapLength_ = AsmJSAllocationGranularity;
}

AsmJSModule::~AsmJSModule()
{
    scriptSource_->decref();

    if (code_) {
        for (unsigned i = 0; i < numExits(); i++) {
            AsmJSModule::ExitDatum &exitDatum = exitIndexToGlobalDatum(i);
            if (!exitDatum.fun)
                continue;

            if (!exitDatum.fun->hasScript())
                continue;

            JSScript *script = exitDatum.fun->nonLazyScript();
            if (!script->hasIonScript())
                continue;

            jit::DependentAsmJSModuleExit exit(this, i);
            script->ionScript()->removeDependentAsmJSModule(exit);
        }

        DeallocateExecutableMemory(code_, pod.totalBytes_);
    }

    for (size_t i = 0; i < numFunctionCounts(); i++)
        js_delete(functionCounts(i));
}

void
AsmJSModule::addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t *asmJSModuleCode,
                           size_t *asmJSModuleData)
{
    *asmJSModuleCode += pod.totalBytes_;
    *asmJSModuleData += mallocSizeOf(this) +
                        globals_.sizeOfExcludingThis(mallocSizeOf) +
                        exits_.sizeOfExcludingThis(mallocSizeOf) +
                        exports_.sizeOfExcludingThis(mallocSizeOf) +
                        heapAccesses_.sizeOfExcludingThis(mallocSizeOf) +
#if defined(MOZ_VTUNE)
                        profiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
#if defined(JS_ION_PERF)
                        profiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
                        perfProfiledBlocksFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
                        functionCounts_.sizeOfExcludingThis(mallocSizeOf);
}

static void
AsmJSModuleObject_finalize(FreeOp *fop, JSObject *obj)
{
    fop->delete_(&obj->as<AsmJSModuleObject>().module());
}

static void
AsmJSModuleObject_trace(JSTracer *trc, JSObject *obj)
{
    obj->as<AsmJSModuleObject>().module().trace(trc);
}

const Class AsmJSModuleObject::class_ = {
    "AsmJSModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(AsmJSModuleObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    nullptr,                 /* convert     */
    AsmJSModuleObject_finalize,
    nullptr,                 /* checkAccess */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    AsmJSModuleObject_trace
};

AsmJSModuleObject *
AsmJSModuleObject::create(ExclusiveContext *cx, ScopedJSDeletePtr<AsmJSModule> *module)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &AsmJSModuleObject::class_, nullptr, nullptr);
    if (!obj)
        return nullptr;

    obj->setReservedSlot(MODULE_SLOT, PrivateValue(module->forget()));
    return &obj->as<AsmJSModuleObject>();
}

AsmJSModule &
AsmJSModuleObject::module() const
{
    JS_ASSERT(is<AsmJSModuleObject>());
    return *(AsmJSModule *)getReservedSlot(MODULE_SLOT).toPrivate();
}

static inline uint8_t *
WriteBytes(uint8_t *dst, const void *src, size_t nbytes)
{
    memcpy(dst, src, nbytes);
    return dst + nbytes;
}

static inline const uint8_t *
ReadBytes(const uint8_t *src, void *dst, size_t nbytes)
{
    memcpy(dst, src, nbytes);
    return src + nbytes;
}

template <class T>
static inline uint8_t *
WriteScalar(uint8_t *dst, T t)
{
    memcpy(dst, &t, sizeof(t));
    return dst + sizeof(t);
}

template <class T>
static inline const uint8_t *
ReadScalar(const uint8_t *src, T *dst)
{
    memcpy(dst, src, sizeof(*dst));
    return src + sizeof(*dst);
}

static size_t
SerializedNameSize(PropertyName *name)
{
    return sizeof(uint32_t) +
           (name ? name->length() * sizeof(jschar) : 0);
}

static uint8_t *
SerializeName(uint8_t *cursor, PropertyName *name)
{
    JS_ASSERT_IF(name, name->length() != 0);
    if (name) {
        cursor = WriteScalar<uint32_t>(cursor, name->length());
        cursor = WriteBytes(cursor, name->chars(), name->length() * sizeof(jschar));
    } else {
        cursor = WriteScalar<uint32_t>(cursor, 0);
    }
    return cursor;
}

static const uint8_t *
DeserializeName(ExclusiveContext *cx, const uint8_t *cursor, PropertyName **name)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);

    if (length == 0) {
        *name = nullptr;
        return cursor;
    }

    Vector<jschar> tmp(cx);
    jschar *src;
    if ((size_t(cursor) & (sizeof(jschar) - 1)) != 0) {
        // Align 'src' for AtomizeChars.
        if (!tmp.resize(length))
            return nullptr;
        memcpy(tmp.begin(), cursor, length * sizeof(jschar));
        src = tmp.begin();
    } else {
        src = (jschar *)cursor;
    }

    JSAtom *atom = AtomizeChars(cx, src, length);
    if (!atom)
        return nullptr;

    *name = atom->asPropertyName();
    return cursor + length * sizeof(jschar);
}

template <class T>
size_t
SerializedVectorSize(const Vector<T, 0, SystemAllocPolicy> &vec)
{
    size_t size = sizeof(uint32_t);
    for (size_t i = 0; i < vec.length(); i++)
        size += vec[i].serializedSize();
    return size;
}

template <class T>
uint8_t *
SerializeVector(uint8_t *cursor, const Vector<T, 0, SystemAllocPolicy> &vec)
{
    cursor = WriteScalar<uint32_t>(cursor, vec.length());
    for (size_t i = 0; i < vec.length(); i++)
        cursor = vec[i].serialize(cursor);
    return cursor;
}

template <class T>
const uint8_t *
DeserializeVector(ExclusiveContext *cx, const uint8_t *cursor, Vector<T, 0, SystemAllocPolicy> *vec)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);
    if (!vec->resize(length))
        return nullptr;
    for (size_t i = 0; i < vec->length(); i++) {
        if (!(cursor = (*vec)[i].deserialize(cx, cursor)))
            return nullptr;
    }
    return cursor;
}

template <class T, class AllocPolicy, class ThisVector>
size_t
SerializedPodVectorSize(const mozilla::VectorBase<T, 0, AllocPolicy, ThisVector> &vec)
{
    return sizeof(uint32_t) +
           vec.length() * sizeof(T);
}

template <class T, class AllocPolicy, class ThisVector>
uint8_t *
SerializePodVector(uint8_t *cursor, const mozilla::VectorBase<T, 0, AllocPolicy, ThisVector> &vec)
{
    cursor = WriteScalar<uint32_t>(cursor, vec.length());
    cursor = WriteBytes(cursor, vec.begin(), vec.length() * sizeof(T));
    return cursor;
}

template <class T, class AllocPolicy, class ThisVector>
const uint8_t *
DeserializePodVector(ExclusiveContext *cx, const uint8_t *cursor,
                     mozilla::VectorBase<T, 0, AllocPolicy, ThisVector> *vec)
{
    uint32_t length;
    cursor = ReadScalar<uint32_t>(cursor, &length);
    if (!vec->resize(length))
        return nullptr;
    cursor = ReadBytes(cursor, vec->begin(), length * sizeof(T));
    return cursor;
}

uint8_t *
AsmJSModule::Global::serialize(uint8_t *cursor) const
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

const uint8_t *
AsmJSModule::Global::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    (cursor = ReadBytes(cursor, &pod, sizeof(pod))) &&
    (cursor = DeserializeName(cx, cursor, &name_));
    return cursor;
}

uint8_t *
AsmJSModule::Exit::serialize(uint8_t *cursor) const
{
    cursor = WriteBytes(cursor, this, sizeof(*this));
    return cursor;
}

size_t
AsmJSModule::Exit::serializedSize() const
{
    return sizeof(*this);
}

const uint8_t *
AsmJSModule::Exit::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    cursor = ReadBytes(cursor, this, sizeof(*this));
    return cursor;
}

uint8_t *
AsmJSModule::ExportedFunction::serialize(uint8_t *cursor) const
{
    cursor = SerializeName(cursor, name_);
    cursor = SerializeName(cursor, maybeFieldName_);
    cursor = SerializePodVector(cursor, argCoercions_);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

size_t
AsmJSModule::ExportedFunction::serializedSize() const
{
    return SerializedNameSize(name_) +
           SerializedNameSize(maybeFieldName_) +
           sizeof(uint32_t) +
           argCoercions_.length() * sizeof(argCoercions_[0]) +
           sizeof(pod);
}

const uint8_t *
AsmJSModule::ExportedFunction::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    (cursor = DeserializeName(cx, cursor, &name_)) &&
    (cursor = DeserializeName(cx, cursor, &maybeFieldName_)) &&
    (cursor = DeserializePodVector(cx, cursor, &argCoercions_)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

size_t
AsmJSModule::serializedSize() const
{
    return sizeof(pod) +
           pod.codeBytes_ +
           SerializedNameSize(globalArgumentName_) +
           SerializedNameSize(importArgumentName_) +
           SerializedNameSize(bufferArgumentName_) +
           SerializedVectorSize(globals_) +
           SerializedVectorSize(exits_) +
           SerializedVectorSize(exports_) +
           SerializedPodVectorSize(heapAccesses_);
}

uint8_t *
AsmJSModule::serialize(uint8_t *cursor) const
{
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    cursor = WriteBytes(cursor, code_, pod.codeBytes_);
    cursor = SerializeName(cursor, globalArgumentName_);
    cursor = SerializeName(cursor, importArgumentName_);
    cursor = SerializeName(cursor, bufferArgumentName_);
    cursor = SerializeVector(cursor, globals_);
    cursor = SerializeVector(cursor, exits_);
    cursor = SerializeVector(cursor, exports_);
    cursor = SerializePodVector(cursor, heapAccesses_);
    return cursor;
}

const uint8_t *
AsmJSModule::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    // To avoid GC-during-deserialization corner cases, prevent atoms from
    // being collected.
    AutoKeepAtoms aka(cx->perThreadData);

    (cursor = ReadBytes(cursor, &pod, sizeof(pod))) &&
    (code_ = AllocateExecutableMemory(cx, pod.totalBytes_)) &&
    (cursor = ReadBytes(cursor, code_, pod.codeBytes_)) &&
    (cursor = DeserializeName(cx, cursor, &globalArgumentName_)) &&
    (cursor = DeserializeName(cx, cursor, &importArgumentName_)) &&
    (cursor = DeserializeName(cx, cursor, &bufferArgumentName_)) &&
    (cursor = DeserializeVector(cx, cursor, &globals_)) &&
    (cursor = DeserializeVector(cx, cursor, &exits_)) &&
    (cursor = DeserializeVector(cx, cursor, &exports_)) &&
    (cursor = DeserializePodVector(cx, cursor, &heapAccesses_));

    loadedFromCache_ = true;
    return cursor;
}

size_t
AsmJSStaticLinkData::serializedSize() const
{
    return sizeof(uint32_t) +
           SerializedPodVectorSize(relativeLinks) +
           SerializedPodVectorSize(absoluteLinks);
}

uint8_t *
AsmJSStaticLinkData::serialize(uint8_t *cursor) const
{
    cursor = WriteScalar<uint32_t>(cursor, operationCallbackExitOffset);
    cursor = SerializePodVector(cursor, relativeLinks);
    cursor = SerializePodVector(cursor, absoluteLinks);
    return cursor;
}

const uint8_t *
AsmJSStaticLinkData::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    (cursor = ReadScalar<uint32_t>(cursor, &operationCallbackExitOffset)) &&
    (cursor = DeserializePodVector(cx, cursor, &relativeLinks)) &&
    (cursor = DeserializePodVector(cx, cursor, &absoluteLinks));
    return cursor;
}

static bool
GetCPUID(uint32_t *cpuId)
{
    enum Arch {
        X86 = 0x1,
        X64 = 0x2,
        ARM = 0x3,
        ARCH_BITS = 2
    };

#if defined(JS_CPU_X86)
    JS_ASSERT(uint32_t(JSC::MacroAssembler::getSSEState()) <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = X86 | (JSC::MacroAssembler::getSSEState() << ARCH_BITS);
    return true;
#elif defined(JS_CPU_X64)
    JS_ASSERT(uint32_t(JSC::MacroAssembler::getSSEState()) <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = X64 | (JSC::MacroAssembler::getSSEState() << ARCH_BITS);
    return true;
#elif defined(JS_CPU_ARM)
    JS_ASSERT(GetARMFlags() <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = ARM | (GetARMFlags() << ARCH_BITS);
    return true;
#else
    return false;
#endif
}

class MachineId
{
    uint32_t cpuId_;
    js::Vector<char> buildId_;

  public:
    MachineId(ExclusiveContext *cx) : buildId_(cx) {}

    bool extractCurrentState(ExclusiveContext *cx) {
        if (!cx->asmJSCacheOps().buildId)
            return false;
        if (!cx->asmJSCacheOps().buildId(&buildId_))
            return false;
        if (!GetCPUID(&cpuId_))
            return false;
        return true;
    }

    size_t serializedSize() const {
        return sizeof(uint32_t) +
               SerializedPodVectorSize(buildId_);
    }

    uint8_t *serialize(uint8_t *cursor) const {
        cursor = WriteScalar<uint32_t>(cursor, cpuId_);
        cursor = SerializePodVector(cursor, buildId_);
        return cursor;
    }

    const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor) {
        (cursor = ReadScalar<uint32_t>(cursor, &cpuId_)) &&
        (cursor = DeserializePodVector(cx, cursor, &buildId_));
        return cursor;
    }

    bool operator==(const MachineId &rhs) const {
        return cpuId_ == rhs.cpuId_ &&
               buildId_.length() == rhs.buildId_.length() &&
               PodEqual(buildId_.begin(), rhs.buildId_.begin(), buildId_.length());
    }
    bool operator!=(const MachineId &rhs) const {
        return !(*this == rhs);
    }
};

struct PropertyNameWrapper
{
    PropertyName *name;

    PropertyNameWrapper()
      : name(nullptr)
    {}
    PropertyNameWrapper(PropertyName *name)
      : name(name)
    {}
    size_t serializedSize() const {
        return SerializedNameSize(name);
    }
    uint8_t *serialize(uint8_t *cursor) const {
        return SerializeName(cursor, name);
    }
    const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor) {
        return DeserializeName(cx, cursor, &name);
    }
};

class ModuleChars
{
    uint32_t length_;
    const jschar *begin_;
    uint32_t isFunCtor_;
    Vector<PropertyNameWrapper, 0, SystemAllocPolicy> funCtorArgs_;

  public:
    bool initFromParsedModule(AsmJSParser &parser, const AsmJSModule &module) {
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
        uint32_t beginOffset = parser.pc->maybeFunction->pn_pos.begin;
        uint32_t endOffset = parser.tokenStream.peekTokenPos().end;
        JS_ASSERT(beginOffset < endOffset);
        begin_ = parser.tokenStream.rawBase() + beginOffset;
        length_ = endOffset - beginOffset;
        isFunCtor_ = parser.pc->isFunctionConstructorBody();
        if (isFunCtor_) {
            unsigned numArgs;
            ParseNode *arg = FunctionArgsList(parser.pc->maybeFunction, &numArgs);
            for (unsigned i = 0; i < numArgs; i++, arg = arg->pn_next) {
                if (!funCtorArgs_.append(arg->name()))
                    return false;
            }
        }
        return true;
    }

    size_t serializedSize() const {
        return sizeof(uint32_t) +
               length_ * sizeof(jschar) +
               sizeof(uint32_t) +
               (isFunCtor_ ? SerializedVectorSize(funCtorArgs_) : 0);
    }

    uint8_t *serialize(uint8_t *cursor) const {
        cursor = WriteScalar<uint32_t>(cursor, length_);
        cursor = WriteBytes(cursor, begin_, length_ * sizeof(jschar));
        cursor = WriteScalar<uint32_t>(cursor, isFunCtor_);
        if (isFunCtor_)
            cursor = SerializeVector(cursor, funCtorArgs_);
        return cursor;
    }

    const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor) {
        cursor = ReadScalar<uint32_t>(cursor, &length_);
        begin_ = reinterpret_cast<const jschar *>(cursor);
        cursor += length_ * sizeof(jschar);
        cursor = ReadScalar<uint32_t>(cursor, &isFunCtor_);
        if (isFunCtor_)
            cursor = DeserializeVector(cx, cursor, &funCtorArgs_);
        return cursor;
    }

    bool matchUnparsedModule(const AsmJSParser &parser) const {
        uint32_t parseBeginOffset = parser.pc->maybeFunction->pn_pos.begin;
        const jschar *parseBegin = parser.tokenStream.rawBase() + parseBeginOffset;
        const jschar *parseLimit = parser.tokenStream.rawLimit();
        JS_ASSERT(parseLimit >= parseBegin);
        if (uint32_t(parseLimit - parseBegin) < length_)
            return false;
        if (!PodEqual(begin_, parseBegin, length_))
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
            if (parseBegin + length_ != parseLimit)
                return false;
            unsigned numArgs;
            ParseNode *arg = FunctionArgsList(parser.pc->maybeFunction, &numArgs);
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

struct ScopedCacheEntryOpenedForWrite
{
    ExclusiveContext *cx;
    const size_t serializedSize;
    uint8_t *memory;
    intptr_t handle;

    ScopedCacheEntryOpenedForWrite(ExclusiveContext *cx, size_t serializedSize)
      : cx(cx), serializedSize(serializedSize), memory(nullptr), handle(-1)
    {}

    ~ScopedCacheEntryOpenedForWrite() {
        if (memory)
            cx->asmJSCacheOps().closeEntryForWrite(cx->global(), serializedSize, memory, handle);
    }
};

void
js::StoreAsmJSModuleInCache(AsmJSParser &parser,
                            const AsmJSModule &module,
                            const AsmJSStaticLinkData &linkData,
                            ExclusiveContext *cx)
{
    MachineId machineId(cx);
    if (!machineId.extractCurrentState(cx))
        return;

    ModuleChars moduleChars;
    if (!moduleChars.initFromParsedModule(parser, module))
        return;

    size_t serializedSize = machineId.serializedSize() +
                            moduleChars.serializedSize() +
                            module.serializedSize() +
                            linkData.serializedSize();

    JS::OpenAsmJSCacheEntryForWriteOp openEntryForWrite = cx->asmJSCacheOps().openEntryForWrite;
    if (!openEntryForWrite)
        return;

    ScopedCacheEntryOpenedForWrite entry(cx, serializedSize);
    if (!openEntryForWrite(cx->global(), entry.serializedSize, &entry.memory, &entry.handle))
        return;

    uint8_t *cursor = entry.memory;
    cursor = machineId.serialize(cursor);
    cursor = moduleChars.serialize(cursor);
    cursor = module.serialize(cursor);
    cursor = linkData.serialize(cursor);

    JS_ASSERT(cursor == entry.memory + serializedSize);
}

struct ScopedCacheEntryOpenedForRead
{
    ExclusiveContext *cx;
    size_t serializedSize;
    const uint8_t *memory;
    intptr_t handle;

    ScopedCacheEntryOpenedForRead(ExclusiveContext *cx)
      : cx(cx), serializedSize(0), memory(nullptr), handle(0)
    {}

    ~ScopedCacheEntryOpenedForRead() {
        if (memory)
            cx->asmJSCacheOps().closeEntryForRead(cx->global(), serializedSize, memory, handle);
    }
};

bool
js::LookupAsmJSModuleInCache(ExclusiveContext *cx,
                             AsmJSParser &parser,
                             ScopedJSDeletePtr<AsmJSModule> *moduleOut,
                             ScopedJSFreePtr<char> *compilationTimeReport)
{
    int64_t usecBefore = PRMJ_Now();

    MachineId machineId(cx);
    if (!machineId.extractCurrentState(cx))
        return true;

    JS::OpenAsmJSCacheEntryForReadOp openEntryForRead = cx->asmJSCacheOps().openEntryForRead;
    if (!openEntryForRead)
        return true;

    ScopedCacheEntryOpenedForRead entry(cx);
    if (!openEntryForRead(cx->global(), &entry.serializedSize, &entry.memory, &entry.handle))
        return true;

    const uint8_t *cursor = entry.memory;

    MachineId cachedMachineId(cx);
    cursor = cachedMachineId.deserialize(cx, cursor);
    if (!cursor)
        return false;
    if (machineId != cachedMachineId)
        return true;

    ModuleChars moduleChars;
    cursor = moduleChars.deserialize(cx, cursor);
    if (!moduleChars.matchUnparsedModule(parser))
        return true;

    ScopedJSDeletePtr<AsmJSModule> module(
        cx->new_<AsmJSModule>(parser.ss, parser.offsetOfCurrentAsmJSModule()));
    if (!module)
        return false;
    cursor = module->deserialize(cx, cursor);
    if (!cursor)
        return false;

    AsmJSStaticLinkData linkData(cx);
    cursor = linkData.deserialize(cx, cursor);
    if (!cursor)
        return false;

    if (cursor != entry.memory + entry.serializedSize)
        MOZ_CRASH("Corrupt serialized module");

    module->staticallyLink(linkData, cx);

    parser.tokenStream.advance(module->charsEnd());

    int64_t usecAfter = PRMJ_Now();
    int ms = (usecAfter - usecBefore) / PRMJ_USEC_PER_MSEC;
    *compilationTimeReport = JS_smprintf("loaded from cache in %dms", ms);
    *moduleOut = module.forget();
    return true;
}
