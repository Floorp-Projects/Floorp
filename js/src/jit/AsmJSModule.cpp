/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSModule.h"

#ifndef XP_WIN
# include <sys/mman.h>
#endif

#include "mozilla/Compression.h"
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
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::Compression::LZ4;

void
AsmJSModule::initHeap(Handle<ArrayBufferObject*> heap, JSContext *cx)
{
    JS_ASSERT(IsValidAsmJSHeapLength(heap->byteLength()));
    JS_ASSERT(dynamicallyLinked_);
    JS_ASSERT(!maybeHeap_);

    maybeHeap_ = heap;
    heapDatum() = heap->dataPointer();

#if defined(JS_CODEGEN_X86)
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
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
    uint32_t heapLength = heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        jit::Assembler::updateBoundsCheck(heapLength,
                                          (jit::Instruction*)(heapAccesses_[i].offset() + code_));
    }
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

namespace js {

// Defined in AsmJS.cpp:

int32_t
InvokeFromAsmJS_Ignore(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

int32_t
InvokeFromAsmJS_ToInt32(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

int32_t
InvokeFromAsmJS_ToNumber(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

}

#if defined(JS_CODEGEN_ARM)
extern "C" {

extern MOZ_EXPORT int64_t
__aeabi_idivmod(int, int);

extern MOZ_EXPORT int64_t
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
RedirectCall(void *fun, ABIFunctionType type)
{
#if defined(JS_ARM_SIMULATOR) || defined(JS_MIPS_SIMULATOR)
    fun = Simulator::RedirectNativeFunction(fun, type);
#endif
    return fun;
}

#ifdef DEBUG
static void
AssumeUnreachable()
{
    MOZ_CRASH("Reached unreachable code in asm.js");
}
#endif

static void *
AddressOf(AsmJSImmKind kind, ExclusiveContext *cx)
{
    switch (kind) {
      case AsmJSImm_Runtime:
        return cx->runtimeAddressForJit();
      case AsmJSImm_StackLimit:
        return cx->stackLimitAddressForJitCode(StackForUntrustedScript);
      case AsmJSImm_ReportOverRecursed:
        return RedirectCall(FuncCast<void (JSContext*)>(js_ReportOverRecursed), Args_General1);
      case AsmJSImm_HandleExecutionInterrupt:
        return RedirectCall(FuncCast(js::HandleExecutionInterrupt), Args_General1);
      case AsmJSImm_InvokeFromAsmJS_Ignore:
        return RedirectCall(FuncCast(InvokeFromAsmJS_Ignore), Args_General4);
      case AsmJSImm_InvokeFromAsmJS_ToInt32:
        return RedirectCall(FuncCast(InvokeFromAsmJS_ToInt32), Args_General4);
      case AsmJSImm_InvokeFromAsmJS_ToNumber:
        return RedirectCall(FuncCast(InvokeFromAsmJS_ToNumber), Args_General4);
      case AsmJSImm_CoerceInPlace_ToInt32:
        return RedirectCall(FuncCast(CoerceInPlace_ToInt32), Args_General2);
      case AsmJSImm_CoerceInPlace_ToNumber:
        return RedirectCall(FuncCast(CoerceInPlace_ToNumber), Args_General2);
      case AsmJSImm_ToInt32:
        return RedirectCall(FuncCast<int32_t (double)>(js::ToInt32), Args_Int_Double);
#if defined(JS_CODEGEN_ARM)
      case AsmJSImm_aeabi_idivmod:
        return RedirectCall(FuncCast(__aeabi_idivmod), Args_General2);
      case AsmJSImm_aeabi_uidivmod:
        return RedirectCall(FuncCast(__aeabi_uidivmod), Args_General2);
#endif
      case AsmJSImm_ModD:
        return RedirectCall(FuncCast(NumberMod), Args_Double_DoubleDouble);
      case AsmJSImm_SinD:
        return RedirectCall(FuncCast<double (double)>(sin), Args_Double_Double);
      case AsmJSImm_CosD:
        return RedirectCall(FuncCast<double (double)>(cos), Args_Double_Double);
      case AsmJSImm_TanD:
        return RedirectCall(FuncCast<double (double)>(tan), Args_Double_Double);
      case AsmJSImm_ASinD:
        return RedirectCall(FuncCast<double (double)>(asin), Args_Double_Double);
      case AsmJSImm_ACosD:
        return RedirectCall(FuncCast<double (double)>(acos), Args_Double_Double);
      case AsmJSImm_ATanD:
        return RedirectCall(FuncCast<double (double)>(atan), Args_Double_Double);
      case AsmJSImm_CeilD:
        return RedirectCall(FuncCast<double (double)>(ceil), Args_Double_Double);
      case AsmJSImm_CeilF:
        return RedirectCall(FuncCast<float (float)>(ceilf), Args_Float32_Float32);
      case AsmJSImm_FloorD:
        return RedirectCall(FuncCast<double (double)>(floor), Args_Double_Double);
      case AsmJSImm_FloorF:
        return RedirectCall(FuncCast<float (float)>(floorf), Args_Float32_Float32);
      case AsmJSImm_ExpD:
        return RedirectCall(FuncCast<double (double)>(exp), Args_Double_Double);
      case AsmJSImm_LogD:
        return RedirectCall(FuncCast<double (double)>(log), Args_Double_Double);
      case AsmJSImm_PowD:
        return RedirectCall(FuncCast(ecmaPow), Args_Double_DoubleDouble);
      case AsmJSImm_ATan2D:
        return RedirectCall(FuncCast(ecmaAtan2), Args_Double_DoubleDouble);
#ifdef DEBUG
      case AsmJSImm_AssumeUnreachable:
        return RedirectCall(FuncCast(AssumeUnreachable), Args_General0);
#endif
      case AsmJSImm_Invalid:
        break;
    }

    MOZ_ASSUME_UNREACHABLE("Bad AsmJSImmKind");
    return nullptr;
}

void
AsmJSModule::restoreToInitialState(ArrayBufferObject *maybePrevBuffer, ExclusiveContext *cx)
{
#ifdef DEBUG
    // Put the absolute links back to -1 so patchDataWithValueCheck assertions
    // in staticallyLink are valid.
    for (size_t i = 0; i < staticLinkData_.absoluteLinks.length(); i++) {
        AbsoluteLink link = staticLinkData_.absoluteLinks[i];
        Assembler::patchDataWithValueCheck(CodeLocationLabel(code_ + link.patchAt.offset()),
                                           PatchedImmPtr((void*)-1),
                                           PatchedImmPtr(AddressOf(link.target, cx)));
    }
#endif

    if (maybePrevBuffer) {
#if defined(JS_CODEGEN_X86)
        // Subtract out the base-pointer added by AsmJSModule::initHeap.
        uint8_t *ptrBase = maybePrevBuffer->dataPointer();
        for (unsigned i = 0; i < heapAccesses_.length(); i++) {
            const jit::AsmJSHeapAccess &access = heapAccesses_[i];
            void *addr = access.patchOffsetAt(code_);
            uint8_t *ptr = reinterpret_cast<uint8_t*>(JSC::X86Assembler::getPointer(addr));
            JS_ASSERT(ptr >= ptrBase);
            JSC::X86Assembler::setPointer(addr, (void *)(ptr - ptrBase));
        }
#endif
    }
}

void
AsmJSModule::setAutoFlushICacheRange()
{
    AutoFlushICache::setRange(uintptr_t(code_), pod.codeBytes_);
}

void
AsmJSModule::staticallyLink(ExclusiveContext *cx)
{
    // Process staticLinkData_

    interruptExit_ = code_ + staticLinkData_.interruptExitOffset;

    for (size_t i = 0; i < staticLinkData_.relativeLinks.length(); i++) {
        RelativeLink link = staticLinkData_.relativeLinks[i];
        *(void **)(code_ + link.patchAtOffset) = code_ + link.targetOffset;
    }

    for (size_t i = 0; i < staticLinkData_.absoluteLinks.length(); i++) {
        AbsoluteLink link = staticLinkData_.absoluteLinks[i];
        Assembler::patchDataWithValueCheck(CodeLocationLabel(code_ + link.patchAt.offset()),
                                           PatchedImmPtr(AddressOf(link.target, cx)),
                                           PatchedImmPtr((void*)-1));
    }

    // Initialize global data segment

    for (size_t i = 0; i < exits_.length(); i++) {
        exitIndexToGlobalDatum(i).exit = interpExitTrampoline(exits_[i]);
        exitIndexToGlobalDatum(i).fun = nullptr;
    }
}

AsmJSModule::AsmJSModule(ScriptSource *scriptSource, uint32_t funcStart,
                         uint32_t offsetToEndOfUseAsm, bool strict)
  : globalArgumentName_(nullptr),
    importArgumentName_(nullptr),
    bufferArgumentName_(nullptr),
    code_(nullptr),
    interruptExit_(nullptr),
    dynamicallyLinked_(false),
    loadedFromCache_(false),
    funcStart_(funcStart),
    offsetToEndOfUseAsm_(offsetToEndOfUseAsm),
    scriptSource_(scriptSource),
    codeIsProtected_(false)
{
    mozilla::PodZero(&pod);
    scriptSource_->incref();
    pod.minHeapLength_ = AsmJSAllocationGranularity;
    pod.strict_ = strict;
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
                        callSites_.sizeOfExcludingThis(mallocSizeOf) +
                        functionNames_.sizeOfExcludingThis(mallocSizeOf) +
                        heapAccesses_.sizeOfExcludingThis(mallocSizeOf) +
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
                        profiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
#if defined(JS_ION_PERF)
                        perfProfiledBlocksFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
                        staticLinkData_.sizeOfExcludingThis(mallocSizeOf);
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

size_t
AsmJSModule::Name::serializedSize() const
{
    return SerializedNameSize(name_);
}

static uint8_t *
SerializeName(uint8_t *cursor, PropertyName *name)
{
    JS_ASSERT_IF(name, !name->empty());
    if (name) {
        cursor = WriteScalar<uint32_t>(cursor, name->length());
        cursor = WriteBytes(cursor, name->chars(), name->length() * sizeof(jschar));
    } else {
        cursor = WriteScalar<uint32_t>(cursor, 0);
    }
    return cursor;
}

uint8_t *
AsmJSModule::Name::serialize(uint8_t *cursor) const
{
    return SerializeName(cursor, name_);
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

    js::Vector<jschar> tmp(cx);
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

const uint8_t *
AsmJSModule::Name::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    return DeserializeName(cx, cursor, &name_);
}

bool
AsmJSModule::Name::clone(ExclusiveContext *cx, Name *out) const
{
    out->name_ = name_;
    return true;
}

template <class T>
size_t
SerializedVectorSize(const js::Vector<T, 0, SystemAllocPolicy> &vec)
{
    size_t size = sizeof(uint32_t);
    for (size_t i = 0; i < vec.length(); i++)
        size += vec[i].serializedSize();
    return size;
}

template <class T>
uint8_t *
SerializeVector(uint8_t *cursor, const js::Vector<T, 0, SystemAllocPolicy> &vec)
{
    cursor = WriteScalar<uint32_t>(cursor, vec.length());
    for (size_t i = 0; i < vec.length(); i++)
        cursor = vec[i].serialize(cursor);
    return cursor;
}

template <class T>
const uint8_t *
DeserializeVector(ExclusiveContext *cx, const uint8_t *cursor, js::Vector<T, 0, SystemAllocPolicy> *vec)
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

template <class T>
bool
CloneVector(ExclusiveContext *cx, const Vector<T, 0, SystemAllocPolicy> &in,
            Vector<T, 0, SystemAllocPolicy> *out)
{
    if (!out->resize(in.length()))
        return false;
    for (size_t i = 0; i < in.length(); i++) {
        if (!in[i].clone(cx, &(*out)[i]))
            return false;
    }
    return true;
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

template <class T>
bool
ClonePodVector(ExclusiveContext *cx, const Vector<T, 0, SystemAllocPolicy> &in,
               Vector<T, 0, SystemAllocPolicy> *out)
{
    if (!out->resize(in.length()))
        return false;
    PodCopy(out->begin(), in.begin(), in.length());
    return true;
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

bool
AsmJSModule::Global::clone(ExclusiveContext *cx, Global *out) const
{
    *out = *this;
    return true;
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

bool
AsmJSModule::Exit::clone(ExclusiveContext *cx, Exit *out) const
{
    *out = *this;
    return true;
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

bool
AsmJSModule::ExportedFunction::clone(ExclusiveContext *cx, ExportedFunction *out) const
{
    out->name_ = name_;
    out->maybeFieldName_ = maybeFieldName_;

    if (!ClonePodVector(cx, argCoercions_, &out->argCoercions_))
        return false;

    out->pod = pod;
    return true;
}

size_t
AsmJSModule::StaticLinkData::serializedSize() const
{
    return sizeof(uint32_t) +
           SerializedPodVectorSize(relativeLinks) +
           SerializedPodVectorSize(absoluteLinks);
}

uint8_t *
AsmJSModule::StaticLinkData::serialize(uint8_t *cursor) const
{
    cursor = WriteScalar<uint32_t>(cursor, interruptExitOffset);
    cursor = SerializePodVector(cursor, relativeLinks);
    cursor = SerializePodVector(cursor, absoluteLinks);
    return cursor;
}

const uint8_t *
AsmJSModule::StaticLinkData::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    (cursor = ReadScalar<uint32_t>(cursor, &interruptExitOffset)) &&
    (cursor = DeserializePodVector(cx, cursor, &relativeLinks)) &&
    (cursor = DeserializePodVector(cx, cursor, &absoluteLinks));
    return cursor;
}

#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
size_t
AsmJSModule::ProfiledFunction::serializedSize() const
{
    return SerializedNameSize(name) +
           sizeof(pod);
}

uint8_t *
AsmJSModule::ProfiledFunction::serialize(uint8_t *cursor) const
{
    cursor = SerializeName(cursor, name);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t *
AsmJSModule::ProfiledFunction::deserialize(ExclusiveContext *cx, const uint8_t *cursor)
{
    (cursor = DeserializeName(cx, cursor, &name)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}
#endif

bool
AsmJSModule::StaticLinkData::clone(ExclusiveContext *cx, StaticLinkData *out) const
{
    out->interruptExitOffset = interruptExitOffset;
    return ClonePodVector(cx, relativeLinks, &out->relativeLinks) &&
           ClonePodVector(cx, absoluteLinks, &out->absoluteLinks);
}

size_t
AsmJSModule::StaticLinkData::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return relativeLinks.sizeOfExcludingThis(mallocSizeOf) +
           absoluteLinks.sizeOfExcludingThis(mallocSizeOf);
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
           SerializedPodVectorSize(callSites_) +
           SerializedVectorSize(functionNames_) +
           SerializedPodVectorSize(heapAccesses_) +
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
           SerializedVectorSize(profiledFunctions_) +
#endif
           staticLinkData_.serializedSize();
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
    cursor = SerializePodVector(cursor, callSites_);
    cursor = SerializeVector(cursor, functionNames_);
    cursor = SerializePodVector(cursor, heapAccesses_);
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    cursor = SerializeVector(cursor, profiledFunctions_);
#endif
    cursor = staticLinkData_.serialize(cursor);
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
    (cursor = DeserializePodVector(cx, cursor, &callSites_)) &&
    (cursor = DeserializeVector(cx, cursor, &functionNames_)) &&
    (cursor = DeserializePodVector(cx, cursor, &heapAccesses_)) &&
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    (cursor = DeserializeVector(cx, cursor, &profiledFunctions_)) &&
#endif
    (cursor = staticLinkData_.deserialize(cx, cursor));

    loadedFromCache_ = true;

    return cursor;
}

// When a module is cloned, we memcpy its executable code. If, right before or
// during the clone, another thread calls AsmJSModule::protectCode() then the
// executable code will become inaccessible. In theory, we could take away only
// PROT_EXEC, but this seems to break emulators.
class AutoUnprotectCodeForClone
{
    JSRuntime *rt_;
    JSRuntime::AutoLockForInterrupt lock_;
    const AsmJSModule &module_;
    const bool protectedBefore_;

  public:
    AutoUnprotectCodeForClone(JSContext *cx, const AsmJSModule &module)
      : rt_(cx->runtime()),
        lock_(rt_),
        module_(module),
        protectedBefore_(module_.codeIsProtected(rt_))
    {
        if (protectedBefore_)
            module_.unprotectCode(rt_);
    }

    ~AutoUnprotectCodeForClone()
    {
        if (protectedBefore_)
            module_.protectCode(rt_);
    }
};

bool
AsmJSModule::clone(JSContext *cx, ScopedJSDeletePtr<AsmJSModule> *moduleOut) const
{
    AutoUnprotectCodeForClone cloneGuard(cx, *this);

    *moduleOut = cx->new_<AsmJSModule>(scriptSource_, funcStart_, offsetToEndOfUseAsm_, pod.strict_);
    if (!*moduleOut)
        return false;

    AsmJSModule &out = **moduleOut;

    // Mirror the order of serialize/deserialize in cloning:

    out.pod = pod;

    out.code_ = AllocateExecutableMemory(cx, pod.totalBytes_);
    if (!out.code_)
        return false;

    memcpy(out.code_, code_, pod.codeBytes_);

    out.globalArgumentName_ = globalArgumentName_;
    out.importArgumentName_ = importArgumentName_;
    out.bufferArgumentName_ = bufferArgumentName_;

    if (!CloneVector(cx, globals_, &out.globals_) ||
        !CloneVector(cx, exits_, &out.exits_) ||
        !CloneVector(cx, exports_, &out.exports_) ||
        !ClonePodVector(cx, callSites_, &out.callSites_) ||
        !CloneVector(cx, functionNames_, &out.functionNames_) ||
        !ClonePodVector(cx, heapAccesses_, &out.heapAccesses_) ||
        !staticLinkData_.clone(cx, &out.staticLinkData_))
    {
        return false;
    }

    out.loadedFromCache_ = loadedFromCache_;

    // We already know the exact extent of areas that need to be patched, just make sure we
    // flush all of them at once.
    out.setAutoFlushICacheRange();

    out.restoreToInitialState(maybeHeap_, cx);
    return true;
}

void
AsmJSModule::protectCode(JSRuntime *rt) const
{
    JS_ASSERT(rt->currentThreadOwnsInterruptLock());

    codeIsProtected_ = true;

    if (!pod.functionBytes_)
        return;

    // Technically, we should be able to only take away the execute permissions,
    // however this seems to break our emulators which don't always check
    // execute permissions while executing code.
#if defined(XP_WIN)
    DWORD oldProtect;
    if (!VirtualProtect(codeBase(), functionBytes(), PAGE_NOACCESS, &oldProtect))
        MOZ_CRASH();
#else  // assume Unix
    if (mprotect(codeBase(), functionBytes(), PROT_NONE))
        MOZ_CRASH();
#endif
}

void
AsmJSModule::unprotectCode(JSRuntime *rt) const
{
    JS_ASSERT(rt->currentThreadOwnsInterruptLock());

    codeIsProtected_ = false;

    if (!pod.functionBytes_)
        return;

#if defined(XP_WIN)
    DWORD oldProtect;
    if (!VirtualProtect(codeBase(), functionBytes(), PAGE_EXECUTE_READWRITE, &oldProtect))
        MOZ_CRASH();
#else  // assume Unix
    if (mprotect(codeBase(), functionBytes(), PROT_READ | PROT_WRITE | PROT_EXEC))
        MOZ_CRASH();
#endif
}

bool
AsmJSModule::codeIsProtected(JSRuntime *rt) const
{
    JS_ASSERT(rt->currentThreadOwnsInterruptLock());
    return codeIsProtected_;
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

#if defined(JS_CODEGEN_X86)
    JS_ASSERT(uint32_t(JSC::MacroAssembler::getSSEState()) <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = X86 | (JSC::MacroAssembler::getSSEState() << ARCH_BITS);
    return true;
#elif defined(JS_CODEGEN_X64)
    JS_ASSERT(uint32_t(JSC::MacroAssembler::getSSEState()) <= (UINT32_MAX >> ARCH_BITS));
    *cpuId = X64 | (JSC::MacroAssembler::getSSEState() << ARCH_BITS);
    return true;
#elif defined(JS_CODEGEN_ARM)
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
    JS::BuildIdCharVector buildId_;

  public:
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
    explicit PropertyNameWrapper(PropertyName *name)
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
  protected:
    uint32_t isFunCtor_;
    js::Vector<PropertyNameWrapper, 0, SystemAllocPolicy> funCtorArgs_;

  public:
    static uint32_t beginOffset(AsmJSParser &parser) {
      return parser.pc->maybeFunction->pn_pos.begin;
    }

    static uint32_t endOffset(AsmJSParser &parser) {
      return parser.tokenStream.peekTokenPos().end;
    }
};

class ModuleCharsForStore : ModuleChars
{
    uint32_t uncompressedSize_;
    uint32_t compressedSize_;
    js::Vector<char, 0, SystemAllocPolicy> compressedBuffer_;

  public:
    bool init(AsmJSParser &parser) {
        JS_ASSERT(beginOffset(parser) < endOffset(parser));

        uncompressedSize_ = (endOffset(parser) - beginOffset(parser)) * sizeof(jschar);
        size_t maxCompressedSize = LZ4::maxCompressedSize(uncompressedSize_);
        if (maxCompressedSize < uncompressedSize_)
            return false;

        if (!compressedBuffer_.resize(maxCompressedSize))
            return false;

        const jschar *chars = parser.tokenStream.rawBase() + beginOffset(parser);
        const char *source = reinterpret_cast<const char*>(chars);
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
               sizeof(uint32_t) +
               compressedSize_ +
               sizeof(uint32_t) +
               (isFunCtor_ ? SerializedVectorSize(funCtorArgs_) : 0);
    }

    uint8_t *serialize(uint8_t *cursor) const {
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
    js::Vector<jschar, 0, SystemAllocPolicy> chars_;

  public:
    const uint8_t *deserialize(ExclusiveContext *cx, const uint8_t *cursor) {
        uint32_t uncompressedSize;
        cursor = ReadScalar<uint32_t>(cursor, &uncompressedSize);

        uint32_t compressedSize;
        cursor = ReadScalar<uint32_t>(cursor, &compressedSize);

        if (!chars_.resize(uncompressedSize / sizeof(jschar)))
            return nullptr;

        const char *source = reinterpret_cast<const char*>(cursor);
        char *dest = reinterpret_cast<char*>(chars_.begin());
        if (!LZ4::decompress(source, dest, uncompressedSize))
            return nullptr;

        cursor += compressedSize;

        cursor = ReadScalar<uint32_t>(cursor, &isFunCtor_);
        if (isFunCtor_)
            cursor = DeserializeVector(cx, cursor, &funCtorArgs_);

        return cursor;
    }

    bool match(AsmJSParser &parser) const {
        const jschar *parseBegin = parser.tokenStream.rawBase() + beginOffset(parser);
        const jschar *parseLimit = parser.tokenStream.rawLimit();
        JS_ASSERT(parseLimit >= parseBegin);
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
            cx->asmJSCacheOps().closeEntryForWrite(serializedSize, memory, handle);
    }
};

bool
js::StoreAsmJSModuleInCache(AsmJSParser &parser,
                            const AsmJSModule &module,
                            ExclusiveContext *cx)
{
    MachineId machineId;
    if (!machineId.extractCurrentState(cx))
        return false;

    ModuleCharsForStore moduleChars;
    if (!moduleChars.init(parser))
        return false;

    size_t serializedSize = machineId.serializedSize() +
                            moduleChars.serializedSize() +
                            module.serializedSize();

    JS::OpenAsmJSCacheEntryForWriteOp open = cx->asmJSCacheOps().openEntryForWrite;
    if (!open)
        return false;

    const jschar *begin = parser.tokenStream.rawBase() + ModuleChars::beginOffset(parser);
    const jschar *end = parser.tokenStream.rawBase() + ModuleChars::endOffset(parser);
    bool installed = parser.options().installedFile;

    ScopedCacheEntryOpenedForWrite entry(cx, serializedSize);
    if (!open(cx->global(), installed, begin, end, entry.serializedSize,
              &entry.memory, &entry.handle)) {
        return false;
    }

    uint8_t *cursor = entry.memory;
    cursor = machineId.serialize(cursor);
    cursor = moduleChars.serialize(cursor);
    cursor = module.serialize(cursor);

    JS_ASSERT(cursor == entry.memory + serializedSize);
    return true;
}

struct ScopedCacheEntryOpenedForRead
{
    ExclusiveContext *cx;
    size_t serializedSize;
    const uint8_t *memory;
    intptr_t handle;

    explicit ScopedCacheEntryOpenedForRead(ExclusiveContext *cx)
      : cx(cx), serializedSize(0), memory(nullptr), handle(0)
    {}

    ~ScopedCacheEntryOpenedForRead() {
        if (memory)
            cx->asmJSCacheOps().closeEntryForRead(serializedSize, memory, handle);
    }
};

bool
js::LookupAsmJSModuleInCache(ExclusiveContext *cx,
                             AsmJSParser &parser,
                             ScopedJSDeletePtr<AsmJSModule> *moduleOut,
                             ScopedJSFreePtr<char> *compilationTimeReport)
{
    int64_t usecBefore = PRMJ_Now();

    MachineId machineId;
    if (!machineId.extractCurrentState(cx))
        return true;

    JS::OpenAsmJSCacheEntryForReadOp open = cx->asmJSCacheOps().openEntryForRead;
    if (!open)
        return true;

    const jschar *begin = parser.tokenStream.rawBase() + ModuleChars::beginOffset(parser);
    const jschar *limit = parser.tokenStream.rawLimit();

    ScopedCacheEntryOpenedForRead entry(cx);
    if (!open(cx->global(), begin, limit, &entry.serializedSize, &entry.memory, &entry.handle))
        return true;

    const uint8_t *cursor = entry.memory;

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

    uint32_t funcStart = parser.pc->maybeFunction->pn_body->pn_pos.begin;
    uint32_t offsetToEndOfUseAsm = parser.tokenStream.currentToken().pos.end;
    bool strict = parser.pc->sc->strict && !parser.pc->sc->hasExplicitUseStrict();
    ScopedJSDeletePtr<AsmJSModule> module(
        cx->new_<AsmJSModule>(parser.ss, funcStart, offsetToEndOfUseAsm, strict));
    if (!module)
        return false;
    cursor = module->deserialize(cx, cursor);

    // No need to flush the instruction cache now, it will be flushed when dynamically linking.
    AutoFlushICache afc("LookupAsmJSModuleInCache", /* inhibit= */ true);
    // We already know the exact extent of areas that need to be patched, just make sure we
    // flush all of them at once.
    module->setAutoFlushICacheRange();

    if (!cursor)
        return false;

    bool atEnd = cursor == entry.memory + entry.serializedSize;
    MOZ_ASSERT(atEnd, "Corrupt cache file");
    if (!atEnd)
        return true;

    module->staticallyLink(cx);

    parser.tokenStream.advance(module->funcEndBeforeCurly());

    int64_t usecAfter = PRMJ_Now();
    int ms = (usecAfter - usecBefore) / PRMJ_USEC_PER_MSEC;
    *compilationTimeReport = JS_smprintf("loaded from cache in %dms", ms);
    *moduleOut = module.forget();
    return true;
}
