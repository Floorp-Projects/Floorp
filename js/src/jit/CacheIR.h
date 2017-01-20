/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CacheIR_h
#define jit_CacheIR_h

#include "mozilla/Maybe.h"

#include "NamespaceImports.h"

#include "gc/Rooting.h"
#include "jit/CompactBuffer.h"
#include "jit/SharedIC.h"

namespace js {
namespace jit {

// CacheIR is an (extremely simple) linear IR language for inline caches.
// From this IR, we can generate machine code for Baseline or Ion IC stubs.
//
// IRWriter
// --------
// CacheIR bytecode is written using IRWriter. This class also records some
// metadata that's used by the Baseline and Ion code generators to generate
// (efficient) machine code.
//
// Sharing Baseline stub code
// --------------------------
// Baseline stores data (like Shape* and fixed slot offsets) inside the ICStub
// structure, instead of embedding them directly in the JitCode. This makes
// Baseline IC code slightly slower, but allows us to share IC code between
// caches. CacheIR makes it easy to share code between stubs: stubs that have
// the same CacheIR (and CacheKind), will have the same Baseline stub code.
//
// Baseline stubs that share JitCode also share a CacheIRStubInfo structure.
// This class stores the CacheIR and the location of GC things stored in the
// stub, for the GC.
//
// JitCompartment has a CacheIRStubInfo* -> JitCode* weak map that's used to
// share both the IR and JitCode between CacheIR stubs. This HashMap owns the
// stubInfo (it uses UniquePtr), so once there are no references left to the
// shared stub code, we can also free the CacheIRStubInfo.
//
// Ion stubs
// ---------
// Unlike Baseline stubs, Ion stubs do not share stub code, and data stored in
// the IonICStub is baked into JIT code. This is one of the reasons Ion stubs
// are faster than Baseline stubs. Also note that Ion ICs contain more state
// (see IonGetPropertyIC for example) and use dynamic input/output registers,
// so sharing stub code for Ion would be much more difficult.

// An OperandId represents either a cache input or a value returned by a
// CacheIR instruction. Most code should use the ValOperandId and ObjOperandId
// classes below. The ObjOperandId class represents an operand that's known to
// be an object.
class OperandId
{
  protected:
    static const uint16_t InvalidId = UINT16_MAX;
    uint16_t id_;

    OperandId() : id_(InvalidId) {}
    explicit OperandId(uint16_t id) : id_(id) {}

  public:
    uint16_t id() const { return id_; }
    bool valid() const { return id_ != InvalidId; }
};

class ValOperandId : public OperandId
{
  public:
    ValOperandId() = default;
    explicit ValOperandId(uint16_t id) : OperandId(id) {}
};

class ObjOperandId : public OperandId
{
  public:
    ObjOperandId() = default;
    explicit ObjOperandId(uint16_t id) : OperandId(id) {}

    bool operator==(const ObjOperandId& other) const { return id_ == other.id_; }
    bool operator!=(const ObjOperandId& other) const { return id_ != other.id_; }
};

class StringOperandId : public OperandId
{
  public:
    StringOperandId() = default;
    explicit StringOperandId(uint16_t id) : OperandId(id) {}
};

class SymbolOperandId : public OperandId
{
  public:
    SymbolOperandId() = default;
    explicit SymbolOperandId(uint16_t id) : OperandId(id) {}
};

class Int32OperandId : public OperandId
{
  public:
    Int32OperandId() = default;
    explicit Int32OperandId(uint16_t id) : OperandId(id) {}
};

class TypedOperandId : public OperandId
{
    JSValueType type_;

  public:
    MOZ_IMPLICIT TypedOperandId(ObjOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_OBJECT)
    {}
    MOZ_IMPLICIT TypedOperandId(StringOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_STRING)
    {}
    MOZ_IMPLICIT TypedOperandId(SymbolOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_SYMBOL)
    {}
    MOZ_IMPLICIT TypedOperandId(Int32OperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_INT32)
    {}

    JSValueType type() const { return type_; }
};

enum class CacheKind : uint8_t
{
    GetProp,
    GetElem,
    GetName,
    SetProp,
};

#define CACHE_IR_OPS(_)                   \
    _(GuardIsObject)                      \
    _(GuardIsObjectOrNull)                \
    _(GuardIsString)                      \
    _(GuardIsSymbol)                      \
    _(GuardIsInt32Index)                  \
    _(GuardType)                          \
    _(GuardShape)                         \
    _(GuardGroup)                         \
    _(GuardProto)                         \
    _(GuardClass)                         \
    _(GuardIsProxy)                       \
    _(GuardNotDOMProxy)                   \
    _(GuardSpecificObject)                \
    _(GuardSpecificAtom)                  \
    _(GuardSpecificSymbol)                \
    _(GuardNoDetachedTypedObjects)        \
    _(GuardMagicValue)                    \
    _(GuardFrameHasNoArgumentsObject)     \
    _(GuardNoDenseElements)               \
    _(GuardNoUnboxedExpando)              \
    _(GuardAndLoadUnboxedExpando)         \
    _(GuardAndGetIndexFromString)         \
    _(LoadObject)                         \
    _(LoadProto)                          \
    _(LoadEnclosingEnvironment)           \
                                          \
    /* See CacheIR.cpp 'DOM proxies' comment. */ \
    _(LoadDOMExpandoValue)                \
    _(LoadDOMExpandoValueGuardGeneration) \
    _(LoadDOMExpandoValueIgnoreGeneration)\
    _(GuardDOMExpandoMissingOrGuardShape) \
                                          \
    _(StoreFixedSlot)                     \
    _(StoreDynamicSlot)                   \
    _(StoreUnboxedProperty)               \
                                          \
    /* The *Result ops load a value into the cache's result register. */ \
    _(LoadFixedSlotResult)                \
    _(LoadDynamicSlotResult)              \
    _(LoadUnboxedPropertyResult)          \
    _(LoadTypedObjectResult)              \
    _(LoadDenseElementResult)             \
    _(LoadDenseElementHoleResult)         \
    _(LoadUnboxedArrayElementResult)      \
    _(LoadTypedElementResult)             \
    _(LoadInt32ArrayLengthResult)         \
    _(LoadUnboxedArrayLengthResult)       \
    _(LoadArgumentsObjectArgResult)       \
    _(LoadArgumentsObjectLengthResult)    \
    _(LoadStringCharResult)               \
    _(LoadStringLengthResult)             \
    _(LoadFrameCalleeResult)              \
    _(LoadFrameNumActualArgsResult)       \
    _(LoadFrameArgumentResult)            \
    _(LoadEnvironmentFixedSlotResult)     \
    _(LoadEnvironmentDynamicSlotResult)   \
    _(CallScriptedGetterResult)           \
    _(CallNativeGetterResult)             \
    _(CallProxyGetResult)                 \
    _(CallProxyGetByValueResult)          \
    _(LoadUndefinedResult)                \
                                          \
    _(TypeMonitorResult)                  \
    _(ReturnFromIC)

enum class CacheOp {
#define DEFINE_OP(op) op,
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP
};

class StubField
{
  public:
    enum class Type : uint8_t {
        // These fields take up a single word.
        RawWord,
        Shape,
        ObjectGroup,
        JSObject,
        Symbol,
        String,
        Id,

        // These fields take up 64 bits on all platforms.
        RawInt64,
        First64BitType = RawInt64,
        DOMExpandoGeneration,
        Value,

        Limit
    };

    static bool sizeIsWord(Type type) {
        MOZ_ASSERT(type != Type::Limit);
        return type < Type::First64BitType;
    }
    static bool sizeIsInt64(Type type) {
        MOZ_ASSERT(type != Type::Limit);
        return type >= Type::First64BitType;
    }
    static size_t sizeInBytes(Type type) {
        if (sizeIsWord(type))
            return sizeof(uintptr_t);
        MOZ_ASSERT(sizeIsInt64(type));
        return sizeof(int64_t);
    }

  private:
    union {
        uintptr_t dataWord_;
        uint64_t dataInt64_;
    };
    Type type_;

  public:
    StubField(uint64_t data, Type type)
      : dataInt64_(data), type_(type)
    {
        MOZ_ASSERT_IF(sizeIsWord(), data <= UINTPTR_MAX);
    }

    Type type() const { return type_; }

    bool sizeIsWord() const { return sizeIsWord(type_); }
    bool sizeIsInt64() const { return sizeIsInt64(type_); }

    uintptr_t asWord() const { MOZ_ASSERT(sizeIsWord()); return dataWord_; }
    uint64_t asInt64() const { MOZ_ASSERT(sizeIsInt64()); return dataInt64_; }
} JS_HAZ_GC_POINTER;

// We use this enum as GuardClass operand, instead of storing Class* pointers
// in the IR, to keep the IR compact and the same size on all platforms.
enum class GuardClassKind : uint8_t
{
    Array,
    UnboxedArray,
    MappedArguments,
    UnmappedArguments,
    WindowProxy,
};

// Class to record CacheIR + some additional metadata for code generation.
class MOZ_RAII CacheIRWriter : public JS::CustomAutoRooter
{
    CompactBufferWriter buffer_;

    uint32_t nextOperandId_;
    uint32_t nextInstructionId_;
    uint32_t numInputOperands_;

    // The data (shapes, slot offsets, etc.) that will be stored in the ICStub.
    Vector<StubField, 8, SystemAllocPolicy> stubFields_;
    size_t stubDataSize_;

    // For each operand id, record which instruction accessed it last. This
    // information greatly improves register allocation.
    Vector<uint32_t, 8, SystemAllocPolicy> operandLastUsed_;

    // OperandId and stub offsets are stored in a single byte, so make sure
    // this doesn't overflow. We use a very conservative limit for now.
    static const size_t MaxOperandIds = 20;
    static const size_t MaxStubDataSizeInBytes = 20 * sizeof(uintptr_t);
    bool tooLarge_;

    void writeOp(CacheOp op) {
        MOZ_ASSERT(uint32_t(op) <= UINT8_MAX);
        buffer_.writeByte(uint32_t(op));
        nextInstructionId_++;
    }

    void writeOperandId(OperandId opId) {
        if (opId.id() < MaxOperandIds) {
            static_assert(MaxOperandIds <= UINT8_MAX, "operand id must fit in a single byte");
            buffer_.writeByte(opId.id());
        } else {
            tooLarge_ = true;
            return;
        }
        if (opId.id() >= operandLastUsed_.length()) {
            buffer_.propagateOOM(operandLastUsed_.resize(opId.id() + 1));
            if (buffer_.oom())
                return;
        }
        MOZ_ASSERT(nextInstructionId_ > 0);
        operandLastUsed_[opId.id()] = nextInstructionId_ - 1;
    }

    void writeOpWithOperandId(CacheOp op, OperandId opId) {
        writeOp(op);
        writeOperandId(opId);
    }

    void addStubField(uint64_t value, StubField::Type fieldType) {
        size_t newStubDataSize = stubDataSize_ + StubField::sizeInBytes(fieldType);
        if (newStubDataSize < MaxStubDataSizeInBytes) {
            buffer_.propagateOOM(stubFields_.append(StubField(value, fieldType)));
            MOZ_ASSERT((stubDataSize_ % sizeof(uintptr_t)) == 0);
            buffer_.writeByte(stubDataSize_ / sizeof(uintptr_t));
            stubDataSize_ = newStubDataSize;
        } else {
            tooLarge_ = true;
        }
    }

    CacheIRWriter(const CacheIRWriter&) = delete;
    CacheIRWriter& operator=(const CacheIRWriter&) = delete;

  public:
    explicit CacheIRWriter(JSContext* cx)
      : CustomAutoRooter(cx),
        nextOperandId_(0),
        nextInstructionId_(0),
        numInputOperands_(0),
        stubDataSize_(0),
        tooLarge_(false)
    {}

    bool failed() const { return buffer_.oom() || tooLarge_; }

    uint32_t numInputOperands() const { return numInputOperands_; }
    uint32_t numOperandIds() const { return nextOperandId_; }
    uint32_t numInstructions() const { return nextInstructionId_; }

    size_t numStubFields() const { return stubFields_.length(); }
    StubField::Type stubFieldType(uint32_t i) const { return stubFields_[i].type(); }

    uint32_t setInputOperandId(uint32_t op) {
        MOZ_ASSERT(op == nextOperandId_);
        nextOperandId_++;
        numInputOperands_++;
        return op;
    }

    void trace(JSTracer* trc) override {
        // For now, assert we only GC before we append stub fields.
        MOZ_RELEASE_ASSERT(stubFields_.empty());
    }

    size_t stubDataSize() const {
        return stubDataSize_;
    }
    void copyStubData(uint8_t* dest) const;
    bool stubDataEqualsMaybeUpdate(uint8_t* stubData) const;

    bool operandIsDead(uint32_t operandId, uint32_t currentInstruction) const {
        if (operandId >= operandLastUsed_.length())
            return false;
        return currentInstruction > operandLastUsed_[operandId];
    }
    const uint8_t* codeStart() const {
        MOZ_ASSERT(!failed());
        return buffer_.buffer();
    }
    const uint8_t* codeEnd() const {
        MOZ_ASSERT(!failed());
        return buffer_.buffer() + buffer_.length();
    }
    uint32_t codeLength() const {
        MOZ_ASSERT(!failed());
        return buffer_.length();
    }

    // This should not be used when compiling Baseline code, as Baseline code
    // shouldn't bake in stub values.
    StubField readStubFieldForIon(size_t i, StubField::Type type) const {
        MOZ_ASSERT(stubFields_[i].type() == type);
        return stubFields_[i];
    }

    ObjOperandId guardIsObject(ValOperandId val) {
        writeOpWithOperandId(CacheOp::GuardIsObject, val);
        return ObjOperandId(val.id());
    }
    StringOperandId guardIsString(ValOperandId val) {
        writeOpWithOperandId(CacheOp::GuardIsString, val);
        return StringOperandId(val.id());
    }
    SymbolOperandId guardIsSymbol(ValOperandId val) {
        writeOpWithOperandId(CacheOp::GuardIsSymbol, val);
        return SymbolOperandId(val.id());
    }
    Int32OperandId guardIsInt32Index(ValOperandId val) {
        Int32OperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::GuardIsInt32Index, val);
        writeOperandId(res);
        return res;
    }
    void guardType(ValOperandId val, JSValueType type) {
        writeOpWithOperandId(CacheOp::GuardType, val);
        static_assert(sizeof(type) == sizeof(uint8_t), "JSValueType should fit in a byte");
        buffer_.writeByte(uint32_t(type));
    }
    void guardIsObjectOrNull(ValOperandId val) {
        writeOpWithOperandId(CacheOp::GuardIsObjectOrNull, val);
    }
    void guardShape(ObjOperandId obj, Shape* shape) {
        writeOpWithOperandId(CacheOp::GuardShape, obj);
        addStubField(uintptr_t(shape), StubField::Type::Shape);
    }
    void guardGroup(ObjOperandId obj, ObjectGroup* group) {
        writeOpWithOperandId(CacheOp::GuardGroup, obj);
        addStubField(uintptr_t(group), StubField::Type::ObjectGroup);
    }
    void guardProto(ObjOperandId obj, JSObject* proto) {
        writeOpWithOperandId(CacheOp::GuardProto, obj);
        addStubField(uintptr_t(proto), StubField::Type::JSObject);
    }
    void guardClass(ObjOperandId obj, GuardClassKind kind) {
        static_assert(sizeof(GuardClassKind) == sizeof(uint8_t),
                      "GuardClassKind must fit in a byte");
        writeOpWithOperandId(CacheOp::GuardClass, obj);
        buffer_.writeByte(uint32_t(kind));
    }
    void guardIsProxy(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::GuardIsProxy, obj);
    }
    void guardNotDOMProxy(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::GuardNotDOMProxy, obj);
    }
    void guardSpecificObject(ObjOperandId obj, JSObject* expected) {
        writeOpWithOperandId(CacheOp::GuardSpecificObject, obj);
        addStubField(uintptr_t(expected), StubField::Type::JSObject);
    }
    void guardSpecificAtom(StringOperandId str, JSAtom* expected) {
        writeOpWithOperandId(CacheOp::GuardSpecificAtom, str);
        addStubField(uintptr_t(expected), StubField::Type::String);
    }
    void guardSpecificSymbol(SymbolOperandId sym, JS::Symbol* expected) {
        writeOpWithOperandId(CacheOp::GuardSpecificSymbol, sym);
        addStubField(uintptr_t(expected), StubField::Type::Symbol);
    }
    void guardMagicValue(ValOperandId val, JSWhyMagic magic) {
        writeOpWithOperandId(CacheOp::GuardMagicValue, val);
        buffer_.writeByte(uint32_t(magic));
    }
    void guardNoDetachedTypedObjects() {
        writeOp(CacheOp::GuardNoDetachedTypedObjects);
    }
    void guardFrameHasNoArgumentsObject() {
        writeOp(CacheOp::GuardFrameHasNoArgumentsObject);
    }

    Int32OperandId guardAndGetIndexFromString(StringOperandId str) {
        Int32OperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::GuardAndGetIndexFromString, str);
        writeOperandId(res);
        return res;
    }

    void loadFrameCalleeResult() {
        writeOp(CacheOp::LoadFrameCalleeResult);
    }
    void loadFrameNumActualArgsResult() {
        writeOp(CacheOp::LoadFrameNumActualArgsResult);
    }
    void loadFrameArgumentResult(Int32OperandId index) {
        writeOpWithOperandId(CacheOp::LoadFrameArgumentResult, index);
    }
    void guardNoDenseElements(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::GuardNoDenseElements, obj);
    }
    void guardNoUnboxedExpando(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::GuardNoUnboxedExpando, obj);
    }
    ObjOperandId guardAndLoadUnboxedExpando(ObjOperandId obj) {
        ObjOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::GuardAndLoadUnboxedExpando, obj);
        writeOperandId(res);
        return res;
    }

    ObjOperandId loadObject(JSObject* obj) {
        ObjOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadObject, res);
        addStubField(uintptr_t(obj), StubField::Type::JSObject);
        return res;
    }
    ObjOperandId loadProto(ObjOperandId obj) {
        ObjOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadProto, obj);
        writeOperandId(res);
        return res;
    }

    ObjOperandId loadEnclosingEnvironment(ObjOperandId obj) {
        ObjOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadEnclosingEnvironment, obj);
        writeOperandId(res);
        return res;
    }

    ValOperandId loadDOMExpandoValue(ObjOperandId obj) {
        ValOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadDOMExpandoValue, obj);
        writeOperandId(res);
        return res;
    }
    void guardDOMExpandoMissingOrGuardShape(ValOperandId expando, Shape* shape) {
        writeOpWithOperandId(CacheOp::GuardDOMExpandoMissingOrGuardShape, expando);
        addStubField(uintptr_t(shape), StubField::Type::Shape);
    }
    ValOperandId loadDOMExpandoValueGuardGeneration(ObjOperandId obj,
                                                    ExpandoAndGeneration* expandoAndGeneration)
    {
        ValOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadDOMExpandoValueGuardGeneration, obj);
        addStubField(uintptr_t(expandoAndGeneration), StubField::Type::RawWord);
        addStubField(expandoAndGeneration->generation, StubField::Type::DOMExpandoGeneration);
        writeOperandId(res);
        return res;
    }
    ValOperandId loadDOMExpandoValueIgnoreGeneration(ObjOperandId obj) {
        ValOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadDOMExpandoValueIgnoreGeneration, obj);
        writeOperandId(res);
        return res;
    }

    void storeFixedSlot(ObjOperandId obj, size_t offset, ValOperandId rhs) {
        writeOpWithOperandId(CacheOp::StoreFixedSlot, obj);
        addStubField(offset, StubField::Type::RawWord);
        writeOperandId(rhs);
    }
    void storeDynamicSlot(ObjOperandId obj, size_t offset, ValOperandId rhs) {
        writeOpWithOperandId(CacheOp::StoreDynamicSlot, obj);
        addStubField(offset, StubField::Type::RawWord);
        writeOperandId(rhs);
    }
    void storeUnboxedProperty(ObjOperandId obj, JSValueType type, size_t offset,
                              ValOperandId rhs)
    {
        writeOpWithOperandId(CacheOp::StoreUnboxedProperty, obj);
        buffer_.writeByte(uint32_t(type));
        addStubField(offset, StubField::Type::RawWord);
        writeOperandId(rhs);
    }

    void loadUndefinedResult() {
        writeOp(CacheOp::LoadUndefinedResult);
    }
    void loadFixedSlotResult(ObjOperandId obj, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadFixedSlotResult, obj);
        addStubField(offset, StubField::Type::RawWord);
    }
    void loadDynamicSlotResult(ObjOperandId obj, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadDynamicSlotResult, obj);
        addStubField(offset, StubField::Type::RawWord);
    }
    void loadUnboxedPropertyResult(ObjOperandId obj, JSValueType type, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadUnboxedPropertyResult, obj);
        buffer_.writeByte(uint32_t(type));
        addStubField(offset, StubField::Type::RawWord);
    }
    void loadTypedObjectResult(ObjOperandId obj, uint32_t offset, TypedThingLayout layout,
                               uint32_t typeDescr) {
        MOZ_ASSERT(uint32_t(layout) <= UINT8_MAX);
        MOZ_ASSERT(typeDescr <= UINT8_MAX);
        writeOpWithOperandId(CacheOp::LoadTypedObjectResult, obj);
        buffer_.writeByte(uint32_t(layout));
        buffer_.writeByte(typeDescr);
        addStubField(offset, StubField::Type::RawWord);
    }
    void loadInt32ArrayLengthResult(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::LoadInt32ArrayLengthResult, obj);
    }
    void loadUnboxedArrayLengthResult(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::LoadUnboxedArrayLengthResult, obj);
    }
    void loadArgumentsObjectArgResult(ObjOperandId obj, Int32OperandId index) {
        writeOpWithOperandId(CacheOp::LoadArgumentsObjectArgResult, obj);
        writeOperandId(index);
    }
    void loadDenseElementResult(ObjOperandId obj, Int32OperandId index) {
        writeOpWithOperandId(CacheOp::LoadDenseElementResult, obj);
        writeOperandId(index);
    }
    void loadDenseElementHoleResult(ObjOperandId obj, Int32OperandId index) {
        writeOpWithOperandId(CacheOp::LoadDenseElementHoleResult, obj);
        writeOperandId(index);
    }
    void loadUnboxedArrayElementResult(ObjOperandId obj, Int32OperandId index, JSValueType elementType) {
        writeOpWithOperandId(CacheOp::LoadUnboxedArrayElementResult, obj);
        writeOperandId(index);
        buffer_.writeByte(uint32_t(elementType));
    }
    void loadTypedElementResult(ObjOperandId obj, Int32OperandId index, TypedThingLayout layout,
                                Scalar::Type elementType) {
        writeOpWithOperandId(CacheOp::LoadTypedElementResult, obj);
        writeOperandId(index);
        buffer_.writeByte(uint32_t(layout));
        buffer_.writeByte(uint32_t(elementType));
    }
    void loadArgumentsObjectLengthResult(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::LoadArgumentsObjectLengthResult, obj);
    }
    void loadStringLengthResult(StringOperandId str) {
        writeOpWithOperandId(CacheOp::LoadStringLengthResult, str);
    }
    void loadStringCharResult(StringOperandId str, Int32OperandId index) {
        writeOpWithOperandId(CacheOp::LoadStringCharResult, str);
        writeOperandId(index);
    }
    void callScriptedGetterResult(ObjOperandId obj, JSFunction* getter) {
        writeOpWithOperandId(CacheOp::CallScriptedGetterResult, obj);
        addStubField(uintptr_t(getter), StubField::Type::JSObject);
    }
    void callNativeGetterResult(ObjOperandId obj, JSFunction* getter) {
        writeOpWithOperandId(CacheOp::CallNativeGetterResult, obj);
        addStubField(uintptr_t(getter), StubField::Type::JSObject);
    }
    void callProxyGetResult(ObjOperandId obj, jsid id) {
        writeOpWithOperandId(CacheOp::CallProxyGetResult, obj);
        addStubField(uintptr_t(JSID_BITS(id)), StubField::Type::Id);
    }
    void callProxyGetByValueResult(ObjOperandId obj, ValOperandId idVal) {
        writeOpWithOperandId(CacheOp::CallProxyGetByValueResult, obj);
        writeOperandId(idVal);
    }
    void loadEnvironmentFixedSlotResult(ObjOperandId obj, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadEnvironmentFixedSlotResult, obj);
        addStubField(offset, StubField::Type::RawWord);
    }
    void loadEnvironmentDynamicSlotResult(ObjOperandId obj, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadEnvironmentDynamicSlotResult, obj);
        addStubField(offset, StubField::Type::RawWord);
    }

    void typeMonitorResult() {
        writeOp(CacheOp::TypeMonitorResult);
    }
    void returnFromIC() {
        writeOp(CacheOp::ReturnFromIC);
    }
};

class CacheIRStubInfo;

// Helper class for reading CacheIR bytecode.
class MOZ_RAII CacheIRReader
{
    CompactBufferReader buffer_;

    CacheIRReader(const CacheIRReader&) = delete;
    CacheIRReader& operator=(const CacheIRReader&) = delete;

  public:
    CacheIRReader(const uint8_t* start, const uint8_t* end)
      : buffer_(start, end)
    {}
    explicit CacheIRReader(const CacheIRWriter& writer)
      : CacheIRReader(writer.codeStart(), writer.codeEnd())
    {}
    explicit CacheIRReader(const CacheIRStubInfo* stubInfo);

    bool more() const { return buffer_.more(); }

    CacheOp readOp() {
        return CacheOp(buffer_.readByte());
    }

    ValOperandId valOperandId() { return ValOperandId(buffer_.readByte()); }
    ObjOperandId objOperandId() { return ObjOperandId(buffer_.readByte()); }
    StringOperandId stringOperandId() { return StringOperandId(buffer_.readByte()); }
    SymbolOperandId symbolOperandId() { return SymbolOperandId(buffer_.readByte()); }
    Int32OperandId int32OperandId() { return Int32OperandId(buffer_.readByte()); }

    uint32_t stubOffset() { return buffer_.readByte() * sizeof(uintptr_t); }
    GuardClassKind guardClassKind() { return GuardClassKind(buffer_.readByte()); }
    JSValueType valueType() { return JSValueType(buffer_.readByte()); }
    TypedThingLayout typedThingLayout() { return TypedThingLayout(buffer_.readByte()); }
    Scalar::Type scalarType() { return Scalar::Type(buffer_.readByte()); }
    uint32_t typeDescrKey() { return buffer_.readByte(); }
    JSWhyMagic whyMagic() { return JSWhyMagic(buffer_.readByte()); }

    bool matchOp(CacheOp op) {
        const uint8_t* pos = buffer_.currentPosition();
        if (readOp() == op)
            return true;
        buffer_.seek(pos, 0);
        return false;
    }
    bool matchOp(CacheOp op, OperandId id) {
        const uint8_t* pos = buffer_.currentPosition();
        if (readOp() == op && buffer_.readByte() == id.id())
            return true;
        buffer_.seek(pos, 0);
        return false;
    }
    bool matchOpEither(CacheOp op1, CacheOp op2) {
        const uint8_t* pos = buffer_.currentPosition();
        CacheOp op = readOp();
        if (op == op1 || op == op2)
            return true;
        buffer_.seek(pos, 0);
        return false;
    }
};

class MOZ_RAII IRGenerator
{
  protected:
    CacheIRWriter writer;
    JSContext* cx_;
    jsbytecode* pc_;
    CacheKind cacheKind_;

    IRGenerator(const IRGenerator&) = delete;
    IRGenerator& operator=(const IRGenerator&) = delete;

    bool maybeGuardInt32Index(const Value& index, ValOperandId indexId,
                              uint32_t* int32Index, Int32OperandId* int32IndexId);

  public:
    explicit IRGenerator(JSContext* cx, jsbytecode* pc, CacheKind cacheKind);

    const CacheIRWriter& writerRef() const { return writer; }
    CacheKind cacheKind() const { return cacheKind_; }
};

enum class CanAttachGetter { Yes, No };

// GetPropIRGenerator generates CacheIR for a GetProp IC.
class MOZ_RAII GetPropIRGenerator : public IRGenerator
{
    HandleValue val_;
    HandleValue idVal_;
    ICStubEngine engine_;
    bool* isTemporarilyUnoptimizable_;
    CanAttachGetter canAttachGetter_;

    enum class PreliminaryObjectAction { None, Unlink, NotePreliminary };
    PreliminaryObjectAction preliminaryObjectAction_;

    bool tryAttachNative(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachUnboxed(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachUnboxedExpando(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachTypedObject(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachObjectLength(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachModuleNamespace(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachWindowProxy(HandleObject obj, ObjOperandId objId, HandleId id);

    bool tryAttachGenericProxy(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachDOMProxyExpando(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachDOMProxyShadowed(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachDOMProxyUnshadowed(HandleObject obj, ObjOperandId objId, HandleId id);
    bool tryAttachProxy(HandleObject obj, ObjOperandId objId, HandleId id);

    bool tryAttachPrimitive(ValOperandId valId, HandleId id);
    bool tryAttachStringChar(ValOperandId valId, ValOperandId indexId);
    bool tryAttachStringLength(ValOperandId valId, HandleId id);
    bool tryAttachMagicArgumentsName(ValOperandId valId, HandleId id);

    bool tryAttachMagicArgument(ValOperandId valId, ValOperandId indexId);
    bool tryAttachArgumentsObjectArg(HandleObject obj, ObjOperandId objId,
                                     uint32_t index, Int32OperandId indexId);

    bool tryAttachDenseElement(HandleObject obj, ObjOperandId objId,
                               uint32_t index, Int32OperandId indexId);
    bool tryAttachDenseElementHole(HandleObject obj, ObjOperandId objId,
                                   uint32_t index, Int32OperandId indexId);
    bool tryAttachUnboxedArrayElement(HandleObject obj, ObjOperandId objId,
                                      uint32_t index, Int32OperandId indexId);
    bool tryAttachTypedElement(HandleObject obj, ObjOperandId objId,
                               uint32_t index, Int32OperandId indexId);

    bool tryAttachProxyElement(HandleObject obj, ObjOperandId objId);

    ValOperandId getElemKeyValueId() const {
        MOZ_ASSERT(cacheKind_ == CacheKind::GetElem);
        return ValOperandId(1);
    }

    // No pc if idempotent, as there can be multiple bytecode locations
    // due to GVN.
    bool idempotent() const { return pc_ == nullptr; }

    // If this is a GetElem cache, emit instructions to guard the incoming Value
    // matches |id|.
    void maybeEmitIdGuard(jsid id);

  public:
    GetPropIRGenerator(JSContext* cx, jsbytecode* pc, CacheKind cacheKind, ICStubEngine engine,
                       bool* isTemporarilyUnoptimizable, HandleValue val, HandleValue idVal,
                       CanAttachGetter canAttachGetter);

    bool tryAttachStub();
    bool tryAttachIdempotentStub();

    bool shouldUnlinkPreliminaryObjectStubs() const {
        return preliminaryObjectAction_ == PreliminaryObjectAction::Unlink;
    }
    bool shouldNotePreliminaryObjectStub() const {
        return preliminaryObjectAction_ == PreliminaryObjectAction::NotePreliminary;
    }
};

// GetPropIRGenerator generates CacheIR for a GetName IC.
class MOZ_RAII GetNameIRGenerator : public IRGenerator
{
    HandleScript script_;
    HandleObject env_;
    HandlePropertyName name_;

    bool tryAttachGlobalNameValue(ObjOperandId objId, HandleId id);
    bool tryAttachGlobalNameGetter(ObjOperandId objId, HandleId id);
    bool tryAttachEnvironmentName(ObjOperandId objId, HandleId id);

  public:
    GetNameIRGenerator(JSContext* cx, jsbytecode* pc, HandleScript script,
                       HandleObject env, HandlePropertyName name);

    bool tryAttachStub();
};

// SetPropIRGenerator generates CacheIR for a SetProp IC.
class MOZ_RAII SetPropIRGenerator : public IRGenerator
{
    HandleValue lhsVal_;
    HandleValue idVal_;
    HandleValue rhsVal_;
    bool* isTemporarilyUnoptimizable_;

    enum class PreliminaryObjectAction { None, Unlink, NotePreliminary };
    PreliminaryObjectAction preliminaryObjectAction_;

    // If Baseline needs an update stub, this contains information to create it.
    RootedId updateStubId_;
    bool needUpdateStub_;

    void setUpdateStubInfo(jsid id) {
        MOZ_ASSERT(!needUpdateStub_);
        needUpdateStub_ = true;
        updateStubId_ = id;
    }

    bool tryAttachNativeSetSlot(HandleObject obj, ObjOperandId objId, HandleId id,
                                 ValOperandId rhsId);
    bool tryAttachUnboxedExpandoSetSlot(HandleObject obj, ObjOperandId objId, HandleId id,
                                        ValOperandId rhsId);
    bool tryAttachUnboxedProperty(HandleObject obj, ObjOperandId objId, HandleId id,
                                  ValOperandId rhsId);

  public:
    SetPropIRGenerator(JSContext* cx, jsbytecode* pc, CacheKind cacheKind,
                       bool* isTemporarilyUnoptimizable, HandleValue lhsVal, HandleValue idVal,
                       HandleValue rhsVal);

    bool tryAttachStub();

    bool shouldUnlinkPreliminaryObjectStubs() const {
        return preliminaryObjectAction_ == PreliminaryObjectAction::Unlink;
    }
    bool shouldNotePreliminaryObjectStub() const {
        return preliminaryObjectAction_ == PreliminaryObjectAction::NotePreliminary;
    }
    jsid updateStubId() const {
        MOZ_ASSERT(needUpdateStub_);
        return updateStubId_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_CacheIR_h */
