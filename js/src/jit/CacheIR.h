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

#define CACHE_IR_OPS(_)                   \
    _(GuardIsObject)                      \
    _(GuardType)                          \
    _(GuardShape)                         \
    _(GuardGroup)                         \
    _(GuardProto)                         \
    _(GuardClass)                         \
    _(GuardSpecificObject)                \
    _(GuardNoDetachedTypedObjects)        \
    _(GuardNoUnboxedExpando)              \
    _(GuardAndLoadUnboxedExpando)         \
    _(LoadObject)                         \
    _(LoadProto)                          \
                                          \
    /* The *Result ops load a value into the cache's result register. */ \
    _(LoadFixedSlotResult)                \
    _(LoadDynamicSlotResult)              \
    _(LoadUnboxedPropertyResult)          \
    _(LoadTypedObjectResult)              \
    _(LoadInt32ArrayLengthResult)         \
    _(LoadUnboxedArrayLengthResult)       \
    _(LoadArgumentsObjectLengthResult)    \
    _(CallScriptedGetterResult)           \
    _(CallNativeGetterResult)             \
    _(LoadUndefinedResult)

enum class CacheOp {
#define DEFINE_OP(op) op,
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP
};

struct StubField {
    enum class GCType {
        NoGCThing,
        Shape,
        ObjectGroup,
        JSObject,
        Limit
    };

    uintptr_t word;
    GCType gcType;

    StubField(uintptr_t word, GCType gcType)
      : word(word), gcType(gcType)
    {}
};

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
class MOZ_RAII CacheIRWriter
{
    CompactBufferWriter buffer_;

    uint32_t nextOperandId_;
    uint32_t nextInstructionId_;
    uint32_t numInputOperands_;

    // The data (shapes, slot offsets, etc.) that will be stored in the ICStub.
    Vector<StubField, 8, SystemAllocPolicy> stubFields_;

    // For each operand id, record which instruction accessed it last. This
    // information greatly improves register allocation.
    Vector<uint32_t, 8, SystemAllocPolicy> operandLastUsed_;

    // OperandId and stub offsets are stored in a single byte, so make sure
    // this doesn't overflow. We use a very conservative limit for now.
    static const size_t MaxOperandIds = 20;
    static const size_t MaxStubFields = 20;
    bool tooLarge_;

    // stubFields_ contains unrooted pointers, so ensure we cannot GC in
    // our scope.
    JS::AutoCheckCannotGC nogc;

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

    void addStubWord(uintptr_t word, StubField::GCType gcType) {
        uint32_t pos = stubFields_.length();
        buffer_.propagateOOM(stubFields_.append(StubField(word, gcType)));
        if (pos < MaxStubFields)
            buffer_.writeByte(pos);
        else
            tooLarge_ = true;
    }

    CacheIRWriter(const CacheIRWriter&) = delete;
    CacheIRWriter& operator=(const CacheIRWriter&) = delete;

  public:
    CacheIRWriter()
      : nextOperandId_(0),
        nextInstructionId_(0),
        numInputOperands_(0),
        tooLarge_(false)
    {}

    bool failed() const { return buffer_.oom() || tooLarge_; }

    uint32_t numInputOperands() const { return numInputOperands_; }
    uint32_t numOperandIds() const { return nextOperandId_; }
    uint32_t numInstructions() const { return nextInstructionId_; }

    size_t numStubFields() const { return stubFields_.length(); }
    StubField::GCType stubFieldGCType(uint32_t i) const { return stubFields_[i].gcType; }

    uint32_t setInputOperandId(uint32_t op) {
        MOZ_ASSERT(op == nextOperandId_);
        nextOperandId_++;
        numInputOperands_++;
        return op;
    }

    size_t stubDataSize() const {
        return stubFields_.length() * sizeof(uintptr_t);
    }
    void copyStubData(uint8_t* dest) const;

    bool operandIsDead(uint32_t operandId, uint32_t currentInstruction) const {
        if (operandId >= operandLastUsed_.length())
            return false;
        return currentInstruction > operandLastUsed_[operandId];
    }
    const uint8_t* codeStart() const {
        return buffer_.buffer();
    }
    const uint8_t* codeEnd() const {
        return buffer_.buffer() + buffer_.length();
    }
    uint32_t codeLength() const {
        return buffer_.length();
    }

    ObjOperandId guardIsObject(ValOperandId val) {
        writeOpWithOperandId(CacheOp::GuardIsObject, val);
        return ObjOperandId(val.id());
    }
    void guardType(ValOperandId val, JSValueType type) {
        writeOpWithOperandId(CacheOp::GuardType, val);
        static_assert(sizeof(type) == sizeof(uint8_t), "JSValueType should fit in a byte");
        buffer_.writeByte(uint32_t(type));
    }
    void guardShape(ObjOperandId obj, Shape* shape) {
        writeOpWithOperandId(CacheOp::GuardShape, obj);
        addStubWord(uintptr_t(shape), StubField::GCType::Shape);
    }
    void guardGroup(ObjOperandId obj, ObjectGroup* group) {
        writeOpWithOperandId(CacheOp::GuardGroup, obj);
        addStubWord(uintptr_t(group), StubField::GCType::ObjectGroup);
    }
    void guardProto(ObjOperandId obj, JSObject* proto) {
        writeOpWithOperandId(CacheOp::GuardProto, obj);
        addStubWord(uintptr_t(proto), StubField::GCType::JSObject);
    }
    void guardClass(ObjOperandId obj, GuardClassKind kind) {
        static_assert(sizeof(GuardClassKind) == sizeof(uint8_t),
                      "GuardClassKind must fit in a byte");
        writeOpWithOperandId(CacheOp::GuardClass, obj);
        buffer_.writeByte(uint32_t(kind));
    }
    void guardSpecificObject(ObjOperandId obj, JSObject* expected) {
        writeOpWithOperandId(CacheOp::GuardSpecificObject, obj);
        addStubWord(uintptr_t(expected), StubField::GCType::JSObject);
    }
    void guardNoDetachedTypedObjects() {
        writeOp(CacheOp::GuardNoDetachedTypedObjects);
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
        addStubWord(uintptr_t(obj), StubField::GCType::JSObject);
        return res;
    }
    ObjOperandId loadProto(ObjOperandId obj) {
        ObjOperandId res(nextOperandId_++);
        writeOpWithOperandId(CacheOp::LoadProto, obj);
        writeOperandId(res);
        return res;
    }

    void loadUndefinedResult() {
        writeOp(CacheOp::LoadUndefinedResult);
    }
    void loadFixedSlotResult(ObjOperandId obj, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadFixedSlotResult, obj);
        addStubWord(offset, StubField::GCType::NoGCThing);
    }
    void loadDynamicSlotResult(ObjOperandId obj, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadDynamicSlotResult, obj);
        addStubWord(offset, StubField::GCType::NoGCThing);
    }
    void loadUnboxedPropertyResult(ObjOperandId obj, JSValueType type, size_t offset) {
        writeOpWithOperandId(CacheOp::LoadUnboxedPropertyResult, obj);
        buffer_.writeByte(uint32_t(type));
        addStubWord(offset, StubField::GCType::NoGCThing);
    }
    void loadTypedObjectResult(ObjOperandId obj, uint32_t offset, TypedThingLayout layout,
                               uint32_t typeDescr) {
        MOZ_ASSERT(uint32_t(layout) <= UINT8_MAX);
        MOZ_ASSERT(typeDescr <= UINT8_MAX);
        writeOpWithOperandId(CacheOp::LoadTypedObjectResult, obj);
        buffer_.writeByte(uint32_t(layout));
        buffer_.writeByte(typeDescr);
        addStubWord(offset, StubField::GCType::NoGCThing);
    }
    void loadInt32ArrayLengthResult(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::LoadInt32ArrayLengthResult, obj);
    }
    void loadUnboxedArrayLengthResult(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::LoadUnboxedArrayLengthResult, obj);
    }
    void loadArgumentsObjectLengthResult(ObjOperandId obj) {
        writeOpWithOperandId(CacheOp::LoadArgumentsObjectLengthResult, obj);
    }
    void callScriptedGetterResult(ObjOperandId obj, JSFunction* getter) {
        writeOpWithOperandId(CacheOp::CallScriptedGetterResult, obj);
        addStubWord(uintptr_t(getter), StubField::GCType::JSObject);
    }
    void callNativeGetterResult(ObjOperandId obj, JSFunction* getter) {
        writeOpWithOperandId(CacheOp::CallNativeGetterResult, obj);
        addStubWord(uintptr_t(getter), StubField::GCType::JSObject);
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

    ValOperandId valOperandId() {
        return ValOperandId(buffer_.readByte());
    }
    ObjOperandId objOperandId() {
        return ObjOperandId(buffer_.readByte());
    }

    uint32_t stubOffset() { return buffer_.readByte(); }
    GuardClassKind guardClassKind() { return GuardClassKind(buffer_.readByte()); }
    JSValueType valueType() { return JSValueType(buffer_.readByte()); }
    TypedThingLayout typedThingLayout() { return TypedThingLayout(buffer_.readByte()); }
    uint32_t typeDescrKey() { return buffer_.readByte(); }

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

// GetPropIRGenerator generates CacheIR for a GetProp IC.
class MOZ_RAII GetPropIRGenerator
{
    JSContext* cx_;
    jsbytecode* pc_;
    HandleValue val_;
    HandlePropertyName name_;
    MutableHandleValue res_;
    ICStubEngine engine_;
    bool* isTemporarilyUnoptimizable_;
    bool emitted_;

    enum class PreliminaryObjectAction { None, Unlink, NotePreliminary };
    PreliminaryObjectAction preliminaryObjectAction_;

    MOZ_MUST_USE bool tryAttachNative(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId);
    MOZ_MUST_USE bool tryAttachUnboxed(CacheIRWriter& writer, HandleObject obj, ObjOperandId objId);
    MOZ_MUST_USE bool tryAttachUnboxedExpando(CacheIRWriter& writer, HandleObject obj,
                                              ObjOperandId objId);
    MOZ_MUST_USE bool tryAttachTypedObject(CacheIRWriter& writer, HandleObject obj,
                                           ObjOperandId objId);
    MOZ_MUST_USE bool tryAttachObjectLength(CacheIRWriter& writer, HandleObject obj,
                                            ObjOperandId objId);
    MOZ_MUST_USE bool tryAttachModuleNamespace(CacheIRWriter& writer, HandleObject obj,
                                               ObjOperandId objId);
    MOZ_MUST_USE bool tryAttachWindowProxy(CacheIRWriter& writer, HandleObject obj,
                                           ObjOperandId objId);

    MOZ_MUST_USE bool tryAttachPrimitive(CacheIRWriter& writer, ValOperandId valId);

    GetPropIRGenerator(const GetPropIRGenerator&) = delete;
    GetPropIRGenerator& operator=(const GetPropIRGenerator&) = delete;

  public:
    GetPropIRGenerator(JSContext* cx, jsbytecode* pc, ICStubEngine engine,
                       bool* isTemporarilyUnoptimizable,
                       HandleValue val, HandlePropertyName name, MutableHandleValue res);

    bool emitted() const { return emitted_; }

    MOZ_MUST_USE bool tryAttachStub(mozilla::Maybe<CacheIRWriter>& writer);

    bool shouldUnlinkPreliminaryObjectStubs() const {
        return preliminaryObjectAction_ == PreliminaryObjectAction::Unlink;
    }
    bool shouldNotePreliminaryObjectStub() const {
        return preliminaryObjectAction_ == PreliminaryObjectAction::NotePreliminary;
    }
};

enum class CacheKind : uint8_t
{
    GetProp
};

} // namespace jit
} // namespace js

#endif /* jit_CacheIR_h */
