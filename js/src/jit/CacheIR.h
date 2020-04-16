/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CacheIR_h
#define jit_CacheIR_h

#include "mozilla/Maybe.h"

#include "NamespaceImports.h"

#include "gc/Rooting.h"
#include "jit/CacheIROpsGenerated.h"
#include "jit/CompactBuffer.h"
#include "jit/ICState.h"
#include "jit/MacroAssembler.h"
#include "jit/Simulator.h"
#include "vm/Iteration.h"
#include "vm/Shape.h"

namespace js {
namespace jit {

enum class BaselineCacheIRStubKind;

// [SMDOC] CacheIR
//
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
// JitZone has a CacheIRStubInfo* -> JitCode* weak map that's used to share both
// the IR and JitCode between Baseline CacheIR stubs. This HashMap owns the
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
// be an object, just as StringOperandId represents a known string, etc.
class OperandId {
 protected:
  static const uint16_t InvalidId = UINT16_MAX;
  uint16_t id_;

  OperandId() : id_(InvalidId) {}
  explicit OperandId(uint16_t id) : id_(id) {}

 public:
  uint16_t id() const { return id_; }
  bool valid() const { return id_ != InvalidId; }
};

class ValOperandId : public OperandId {
 public:
  ValOperandId() = default;
  explicit ValOperandId(uint16_t id) : OperandId(id) {}
};

class ValueTagOperandId : public OperandId {
 public:
  ValueTagOperandId() = default;
  explicit ValueTagOperandId(uint16_t id) : OperandId(id) {}
};

class ObjOperandId : public OperandId {
 public:
  ObjOperandId() = default;
  explicit ObjOperandId(uint16_t id) : OperandId(id) {}

  bool operator==(const ObjOperandId& other) const { return id_ == other.id_; }
  bool operator!=(const ObjOperandId& other) const { return id_ != other.id_; }
};

class NumberOperandId : public ValOperandId {
 public:
  NumberOperandId() = default;
  explicit NumberOperandId(uint16_t id) : ValOperandId(id) {}
};

class StringOperandId : public OperandId {
 public:
  StringOperandId() = default;
  explicit StringOperandId(uint16_t id) : OperandId(id) {}
};

class SymbolOperandId : public OperandId {
 public:
  SymbolOperandId() = default;
  explicit SymbolOperandId(uint16_t id) : OperandId(id) {}
};

class BigIntOperandId : public OperandId {
 public:
  BigIntOperandId() = default;
  explicit BigIntOperandId(uint16_t id) : OperandId(id) {}
};

class Int32OperandId : public OperandId {
 public:
  Int32OperandId() = default;
  explicit Int32OperandId(uint16_t id) : OperandId(id) {}
};

class TypedOperandId : public OperandId {
  JSValueType type_;

 public:
  MOZ_IMPLICIT TypedOperandId(ObjOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_OBJECT) {}
  MOZ_IMPLICIT TypedOperandId(StringOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_STRING) {}
  MOZ_IMPLICIT TypedOperandId(SymbolOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_SYMBOL) {}
  MOZ_IMPLICIT TypedOperandId(BigIntOperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_BIGINT) {}
  MOZ_IMPLICIT TypedOperandId(Int32OperandId id)
      : OperandId(id.id()), type_(JSVAL_TYPE_INT32) {}
  MOZ_IMPLICIT TypedOperandId(ValueTagOperandId val)
      : OperandId(val.id()), type_(JSVAL_TYPE_UNKNOWN) {}
  TypedOperandId(ValOperandId val, JSValueType type)
      : OperandId(val.id()), type_(type) {}

  JSValueType type() const { return type_; }
};

#define CACHE_IR_KINDS(_) \
  _(GetProp)              \
  _(GetElem)              \
  _(GetName)              \
  _(GetPropSuper)         \
  _(GetElemSuper)         \
  _(GetIntrinsic)         \
  _(SetProp)              \
  _(SetElem)              \
  _(BindName)             \
  _(In)                   \
  _(HasOwn)               \
  _(TypeOf)               \
  _(InstanceOf)           \
  _(GetIterator)          \
  _(Compare)              \
  _(ToBool)               \
  _(Call)                 \
  _(UnaryArith)           \
  _(BinaryArith)          \
  _(NewObject)

enum class CacheKind : uint8_t {
#define DEFINE_KIND(kind) kind,
  CACHE_IR_KINDS(DEFINE_KIND)
#undef DEFINE_KIND
};

extern const char* const CacheKindNames[];

// This namespace exists to make it possible to use unqualified
// argument types in CACHE_IR_OPS without letting the symbols escape
// into the global namespace. Any code that consumes the argument
// information must have CacheIROpFormat in scope.
namespace CacheIROpFormat {
enum ArgType {
  None,
  Id,
  Field,
  Byte,
  Int32,
  UInt32,
  Word,
};

extern const uint32_t ArgLengths[];
}  // namespace CacheIROpFormat

enum class CacheOp {
#define DEFINE_OP(op, ...) op,
  CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP
};

const char* const CacheIrOpNames[] = {
#define OPNAME(op, ...) #op,
    CACHE_IR_OPS(OPNAME)
#undef OPNAME
};

class StubField {
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
    if (sizeIsWord(type)) {
      return sizeof(uintptr_t);
    }
    MOZ_ASSERT(sizeIsInt64(type));
    return sizeof(int64_t);
  }

 private:
  uint64_t data_;
  Type type_;

 public:
  StubField(uint64_t data, Type type) : data_(data), type_(type) {
    MOZ_ASSERT_IF(sizeIsWord(), data <= UINTPTR_MAX);
  }

  Type type() const { return type_; }

  bool sizeIsWord() const { return sizeIsWord(type_); }
  bool sizeIsInt64() const { return sizeIsInt64(type_); }

  uintptr_t asWord() const {
    MOZ_ASSERT(sizeIsWord());
    return uintptr_t(data_);
  }
  uint64_t asInt64() const {
    MOZ_ASSERT(sizeIsInt64());
    return data_;
  }
} JS_HAZ_GC_POINTER;

using FieldOffset = uint8_t;

// This class is used to wrap up information about a call to make it
// easier to convey from one function to another. (In particular,
// CacheIRWriter encodes the CallFlags in CacheIR, and CacheIRReader
// decodes them and uses them for compilation.)
class CallFlags {
 public:
  enum ArgFormat : uint8_t {
    Standard,
    Spread,
    FunCall,
    FunApplyArgs,
    FunApplyArray,
    LastArgFormat = FunApplyArray
  };

  CallFlags(bool isConstructing, bool isSpread, bool isSameRealm = false)
      : argFormat_(isSpread ? Spread : Standard),
        isConstructing_(isConstructing),
        isSameRealm_(isSameRealm) {}
  explicit CallFlags(ArgFormat format)
      : argFormat_(format), isConstructing_(false), isSameRealm_(false) {}

  ArgFormat getArgFormat() const { return argFormat_; }
  bool isConstructing() const {
    MOZ_ASSERT_IF(isConstructing_,
                  argFormat_ == Standard || argFormat_ == Spread);
    return isConstructing_;
  }
  bool isSameRealm() const { return isSameRealm_; }

 private:
  ArgFormat argFormat_;
  bool isConstructing_;
  bool isSameRealm_;

  // Used for encoding/decoding
  static const uint8_t ArgFormatBits = 4;
  static const uint8_t ArgFormatMask = (1 << ArgFormatBits) - 1;
  static_assert(LastArgFormat <= ArgFormatMask, "Not enough arg format bits");
  static const uint8_t IsConstructing = 1 << 5;
  static const uint8_t IsSameRealm = 1 << 6;

  friend class CacheIRReader;
  friend class CacheIRWriter;
};

enum class AttachDecision {
  // We cannot attach a stub.
  NoAction,

  // We can attach a stub.
  Attach,

  // We cannot currently attach a stub, but we expect to be able to do so in the
  // future. In this case, we do not call trackNotAttached().
  TemporarilyUnoptimizable,

  // We want to attach a stub, but the result of the operation is
  // needed to generate that stub. For example, AddSlot needs to know
  // the resulting shape. Note: the attached stub will inspect the
  // inputs to the operation, so most input checks should be done
  // before the actual operation, with only minimal checks remaining
  // for the deferred portion. This prevents arbitrary scripted code
  // run by the operation from interfering with the conditions being
  // checked.
  Deferred
};

// If the input expression evaluates to an AttachDecision other than NoAction,
// return that AttachDecision. If it is NoAction, do nothing.
#define TRY_ATTACH(expr)                                    \
  do {                                                      \
    AttachDecision tryAttachTempResult_ = expr;             \
    if (tryAttachTempResult_ != AttachDecision::NoAction) { \
      return tryAttachTempResult_;                          \
    }                                                       \
  } while (0)

// Set of arguments supported by GetIndexOfArgument.
// Support for Arg2 and up can be added easily, but is currently unneeded.
enum class ArgumentKind : uint8_t { Callee, This, NewTarget, Arg0, Arg1 };

// This function calculates the index of an argument based on the call flags.
// addArgc is an out-parameter, indicating whether the value of argc should
// be added to the return value to find the actual index.
inline int32_t GetIndexOfArgument(ArgumentKind kind, CallFlags flags,
                                  bool* addArgc) {
  // *** STACK LAYOUT (bottom to top) ***        ******** INDEX ********
  //   Callee                                <-- argc+1 + isConstructing
  //   ThisValue                             <-- argc   + isConstructing
  //   Args: | Arg0 |        |  ArgArray  |  <-- argc-1 + isConstructing
  //         | Arg1 | --or-- |            |  <-- argc-2 + isConstructing
  //         | ...  |        | (if spread |  <-- ...
  //         | ArgN |        |  call)     |  <-- 0      + isConstructing
  //   NewTarget (only if constructing)      <-- 0 (if it exists)
  //
  // If this is a spread call, then argc is always 1, and we can calculate the
  // index directly. If this is not a spread call, then the index of any
  // argument other than NewTarget depends on argc.

  // First we determine whether the caller needs to add argc.
  switch (flags.getArgFormat()) {
    case CallFlags::Standard:
      *addArgc = true;
      break;
    case CallFlags::Spread:
      // Spread calls do not have Arg1 or higher.
      MOZ_ASSERT(kind != ArgumentKind::Arg1);
      *addArgc = false;
      break;
    case CallFlags::FunCall:
    case CallFlags::FunApplyArgs:
    case CallFlags::FunApplyArray:
      MOZ_CRASH("Currently unreachable");
      break;
  }

  // Second, we determine the offset relative to argc.
  bool hasArgumentArray = !*addArgc;
  switch (kind) {
    case ArgumentKind::Callee:
      return flags.isConstructing() + hasArgumentArray + 1;
    case ArgumentKind::This:
      return flags.isConstructing() + hasArgumentArray;
    case ArgumentKind::Arg0:
      return flags.isConstructing() + hasArgumentArray - 1;
    case ArgumentKind::Arg1:
      return flags.isConstructing() + hasArgumentArray - 2;
    case ArgumentKind::NewTarget:
      MOZ_ASSERT(flags.isConstructing());
      *addArgc = false;
      return 0;
    default:
      MOZ_CRASH("Invalid argument kind");
  }
}

// We use this enum as GuardClass operand, instead of storing Class* pointers
// in the IR, to keep the IR compact and the same size on all platforms.
enum class GuardClassKind : uint8_t {
  Array,
  MappedArguments,
  UnmappedArguments,
  WindowProxy,
  JSFunction,
};

// Some ops refer to shapes that might be in other zones. Instead of putting
// cross-zone pointers in the caches themselves (which would complicate tracing
// enormously), these ops instead contain wrappers for objects in the target
// zone, which refer to the actual shape via a reserved slot.
JSObject* NewWrapperWithObjectShape(JSContext* cx, HandleNativeObject obj);

// Enum for stubs handling a combination of typed arrays and typed objects.
enum TypedThingLayout {
  Layout_TypedArray,
  Layout_OutlineTypedObject,
  Layout_InlineTypedObject
};

void LoadShapeWrapperContents(MacroAssembler& masm, Register obj, Register dst,
                              Label* failure);

enum class MetaTwoByteKind : uint8_t {
  NativeTemplateObject,
  ScriptedTemplateObject,
};

#ifdef JS_SIMULATOR
bool CallAnyNative(JSContext* cx, unsigned argc, Value* vp);
#endif

// Class to record CacheIR + some additional metadata for code generation.
class MOZ_RAII CacheIRWriter : public JS::CustomAutoRooter {
  JSContext* cx_;
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

  // Basic caching to avoid quadatic lookup behaviour in readStubFieldForIon.
  mutable uint32_t lastOffset_;
  mutable uint32_t lastIndex_;

  void assertSameCompartment(JSObject*);

  void writeOp(CacheOp op) {
    MOZ_ASSERT(uint32_t(op) <= UINT8_MAX);
    buffer_.writeByte(uint32_t(op));
    nextInstructionId_++;
  }

  void writeOperandId(OperandId opId) {
    if (opId.id() < MaxOperandIds) {
      static_assert(MaxOperandIds <= UINT8_MAX,
                    "operand id must fit in a single byte");
      buffer_.writeByte(opId.id());
    } else {
      tooLarge_ = true;
      return;
    }
    if (opId.id() >= operandLastUsed_.length()) {
      buffer_.propagateOOM(operandLastUsed_.resize(opId.id() + 1));
      if (buffer_.oom()) {
        return;
      }
    }
    MOZ_ASSERT(nextInstructionId_ > 0);
    operandLastUsed_[opId.id()] = nextInstructionId_ - 1;
  }

  void writeInt32Immediate(int32_t i32) { buffer_.writeFixedUint32_t(i32); }
  void writeUint32Immediate(uint32_t u32) { buffer_.writeFixedUint32_t(u32); }
  void writePointer(void* ptr) { buffer_.writeRawPointer(ptr); }

  void writeCallFlags(CallFlags flags) {
    // See CacheIRReader::callFlags()
    uint8_t value = flags.getArgFormat();
    if (flags.isConstructing()) {
      value |= CallFlags::IsConstructing;
    }
    if (flags.isSameRealm()) {
      value |= CallFlags::IsSameRealm;
    }
    buffer_.writeByte(uint32_t(value));
  }

  void writeOpWithOperandId(CacheOp op, OperandId opId) {
    writeOp(op);
    writeOperandId(opId);
  }

  uint8_t addStubField(uint64_t value, StubField::Type fieldType) {
    uint8_t offset = 0;
    size_t newStubDataSize = stubDataSize_ + StubField::sizeInBytes(fieldType);
    if (newStubDataSize < MaxStubDataSizeInBytes) {
      buffer_.propagateOOM(stubFields_.append(StubField(value, fieldType)));
      MOZ_ASSERT((stubDataSize_ % sizeof(uintptr_t)) == 0);
      offset = stubDataSize_ / sizeof(uintptr_t);
      buffer_.writeByte(offset);
      stubDataSize_ = newStubDataSize;
    } else {
      tooLarge_ = true;
    }
    return offset;
  }

  void reuseStubField(FieldOffset offset) {
    MOZ_ASSERT(offset < stubDataSize_ / sizeof(uintptr_t));
    buffer_.writeByte(offset);
  }

  CacheIRWriter(const CacheIRWriter&) = delete;
  CacheIRWriter& operator=(const CacheIRWriter&) = delete;

 public:
  explicit CacheIRWriter(JSContext* cx)
      : CustomAutoRooter(cx),
        cx_(cx),
        nextOperandId_(0),
        nextInstructionId_(0),
        numInputOperands_(0),
        stubDataSize_(0),
        tooLarge_(false),
        lastOffset_(0),
        lastIndex_(0) {}

  bool failed() const { return buffer_.oom() || tooLarge_; }

  uint32_t numInputOperands() const { return numInputOperands_; }
  uint32_t numOperandIds() const { return nextOperandId_; }
  uint32_t numInstructions() const { return nextInstructionId_; }

  size_t numStubFields() const { return stubFields_.length(); }
  StubField::Type stubFieldType(uint32_t i) const {
    return stubFields_[i].type();
  }

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

  size_t stubDataSize() const { return stubDataSize_; }
  void copyStubData(uint8_t* dest) const;
  bool stubDataEqualsMaybeUpdate(uint8_t* stubData, bool* updated) const;

  bool operandIsDead(uint32_t operandId, uint32_t currentInstruction) const {
    if (operandId >= operandLastUsed_.length()) {
      return false;
    }
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
  StubField readStubFieldForIon(uint32_t offset, StubField::Type type) const;

  ObjOperandId guardToObject(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardToObject, val);
    return ObjOperandId(val.id());
  }

  Int32OperandId guardToBoolean(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardToBoolean, val);
    writeOperandId(res);
    return res;
  }

  StringOperandId guardToString(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardToString, val);
    return StringOperandId(val.id());
  }

  SymbolOperandId guardToSymbol(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardToSymbol, val);
    return SymbolOperandId(val.id());
  }

  BigIntOperandId guardToBigInt(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardToBigInt, val);
    return BigIntOperandId(val.id());
  }

  Int32OperandId guardToInt32(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardToInt32, val);
    writeOperandId(res);
    return res;
  }

  Int32OperandId guardToInt32Index(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardToInt32Index, val);
    writeOperandId(res);
    return res;
  }

  Int32OperandId guardToTypedArrayIndex(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardToTypedArrayIndex, val);
    writeOperandId(res);
    return res;
  }

  Int32OperandId guardToInt32ModUint32(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardToInt32ModUint32, val);
    writeOperandId(res);
    return res;
  }

  Int32OperandId guardToUint8Clamped(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardToUint8Clamped, val);
    writeOperandId(res);
    return res;
  }

  NumberOperandId guardIsNumber(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardIsNumber, val);
    return NumberOperandId(val.id());
  }

  void guardType(ValOperandId val, ValueType type) {
    writeOpWithOperandId(CacheOp::GuardType, val);
    static_assert(sizeof(type) == sizeof(uint8_t),
                  "JS::ValueType should fit in a byte");
    buffer_.writeByte(uint32_t(type));
  }

  void guardIsObjectOrNull(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardIsObjectOrNull, val);
  }

  void guardIsNullOrUndefined(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardIsNullOrUndefined, val);
  }

  void guardIsNotNullOrUndefined(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardIsNotNullOrUndefined, val);
  }

  void guardIsNull(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardIsNull, val);
  }

  void guardIsUndefined(ValOperandId val) {
    writeOpWithOperandId(CacheOp::GuardIsUndefined, val);
  }

  void guardShape(ObjOperandId obj, Shape* shape) {
    MOZ_ASSERT(shape);
    writeOpWithOperandId(CacheOp::GuardShape, obj);
    addStubField(uintptr_t(shape), StubField::Type::Shape);
  }

  void guardShapeForClass(ObjOperandId obj, Shape* shape) {
    // Guard shape to ensure that object class is unchanged. This is true
    // for all shapes.
    guardShape(obj, shape);
  }

  void guardShapeForOwnProperties(ObjOperandId obj, Shape* shape) {
    // Guard shape to detect changes to (non-dense) own properties. This
    // also implies |guardShapeForClass|.
    MOZ_ASSERT(shape->getObjectClass()->isNative());
    guardShape(obj, shape);
  }

  void guardXrayExpandoShapeAndDefaultProto(ObjOperandId obj,
                                            JSObject* shapeWrapper) {
    assertSameCompartment(shapeWrapper);
    writeOpWithOperandId(CacheOp::GuardXrayExpandoShapeAndDefaultProto, obj);
    buffer_.writeByte(uint32_t(!!shapeWrapper));
    addStubField(uintptr_t(shapeWrapper), StubField::Type::JSObject);
  }

  // Guard rhs[slot] == prototypeObject
  void guardFunctionPrototype(ObjOperandId rhs, uint32_t slot,
                              ObjOperandId protoId) {
    writeOpWithOperandId(CacheOp::GuardFunctionPrototype, rhs);
    writeOperandId(protoId);
    addStubField(slot, StubField::Type::RawWord);
  }

  void guardNoAllocationMetadataBuilder() {
    writeOp(CacheOp::GuardNoAllocationMetadataBuilder);
  }

  void guardObjectGroupNotPretenured(ObjectGroup* group) {
    writeOp(CacheOp::GuardObjectGroupNotPretenured);
    addStubField(uintptr_t(group), StubField::Type::ObjectGroup);
  }

  void guardFunctionHasJitEntry(ObjOperandId fun, bool isConstructing) {
    writeOpWithOperandId(CacheOp::GuardFunctionHasJitEntry, fun);
    buffer_.writeByte(isConstructing);
  }

  void guardNotClassConstructor(ObjOperandId fun) {
    writeOpWithOperandId(CacheOp::GuardNotClassConstructor, fun);
  }

 public:
  // Use (or create) a specialization below to clarify what constaint the
  // group guard is implying.
  void guardGroup(ObjOperandId obj, ObjectGroup* group) {
    writeOpWithOperandId(CacheOp::GuardGroup, obj);
    addStubField(uintptr_t(group), StubField::Type::ObjectGroup);
  }

  void guardGroupForProto(ObjOperandId obj, ObjectGroup* group) {
    MOZ_ASSERT(!group->hasUncacheableProto());
    guardGroup(obj, group);
  }

  void guardGroupForTypeBarrier(ObjOperandId obj, ObjectGroup* group) {
    // Typesets will always be a super-set of any typesets previously seen
    // for this group. If the type/group of a value being stored to a
    // property in this group is not known, a TypeUpdate IC chain should be
    // used as well.
    guardGroup(obj, group);
  }

  void guardGroupForLayout(ObjOperandId obj, ObjectGroup* group) {
    // NOTE: Comment in guardGroupForTypeBarrier also applies.
    MOZ_ASSERT(IsTypedObjectClass(group->clasp()));
    guardGroup(obj, group);
  }

  void guardProto(ObjOperandId obj, JSObject* proto) {
    assertSameCompartment(proto);
    writeOpWithOperandId(CacheOp::GuardProto, obj);
    addStubField(uintptr_t(proto), StubField::Type::JSObject);
  }

  void guardClass(ObjOperandId obj, GuardClassKind kind) {
    static_assert(sizeof(GuardClassKind) == sizeof(uint8_t),
                  "GuardClassKind must fit in a byte");
    writeOpWithOperandId(CacheOp::GuardClass, obj);
    buffer_.writeByte(uint32_t(kind));
  }

  void guardAnyClass(ObjOperandId obj, const JSClass* clasp) {
    writeOpWithOperandId(CacheOp::GuardAnyClass, obj);
    addStubField(uintptr_t(clasp), StubField::Type::RawWord);
  }

  void guardFunctionIsNative(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardFunctionIsNative, obj);
  }

  void guardFunctionIsConstructor(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardFunctionIsConstructor, obj);
  }

  void guardSpecificNativeFunction(ObjOperandId obj, JSNative nativeFunc) {
    writeOpWithOperandId(CacheOp::GuardSpecificNativeFunction, obj);
    writePointer(JS_FUNC_TO_DATA_PTR(void*, nativeFunc));
  }

  void guardIsNativeObject(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardIsNativeObject, obj);
  }

  void guardIsProxy(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardIsProxy, obj);
  }

  void guardHasProxyHandler(ObjOperandId obj, const void* handler) {
    writeOpWithOperandId(CacheOp::GuardHasProxyHandler, obj);
    addStubField(uintptr_t(handler), StubField::Type::RawWord);
  }

  void guardNotDOMProxy(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardNotDOMProxy, obj);
  }

  FieldOffset guardSpecificObject(ObjOperandId obj, JSObject* expected) {
    assertSameCompartment(expected);
    writeOpWithOperandId(CacheOp::GuardSpecificObject, obj);
    return addStubField(uintptr_t(expected), StubField::Type::JSObject);
  }

  FieldOffset guardSpecificFunction(ObjOperandId obj, JSFunction* expected) {
    // Guard object is a specific function. This implies immutable fields on
    // the JSFunction struct itself are unchanged.
    return guardSpecificObject(obj, expected);
  }

  FieldOffset guardSpecificAtom(StringOperandId str, JSAtom* expected) {
    writeOpWithOperandId(CacheOp::GuardSpecificAtom, str);
    return addStubField(uintptr_t(expected), StubField::Type::String);
  }

  void guardSpecificSymbol(SymbolOperandId sym, JS::Symbol* expected) {
    writeOpWithOperandId(CacheOp::GuardSpecificSymbol, sym);
    addStubField(uintptr_t(expected), StubField::Type::Symbol);
  }

  void guardSpecificInt32Immediate(Int32OperandId operand, int32_t expected) {
    writeOpWithOperandId(CacheOp::GuardSpecificInt32Immediate, operand);
    writeInt32Immediate(expected);
  }

  void guardMagicValue(ValOperandId val, JSWhyMagic magic) {
    writeOpWithOperandId(CacheOp::GuardMagicValue, val);
    buffer_.writeByte(uint32_t(magic));
  }

  void guardCompartment(ObjOperandId obj, JSObject* global,
                        JS::Compartment* compartment) {
    assertSameCompartment(global);
    writeOpWithOperandId(CacheOp::GuardCompartment, obj);
    // Add a reference to a global in the compartment to keep it alive.
    addStubField(uintptr_t(global), StubField::Type::JSObject);
    // Use RawWord, because compartments never move and it can't be GCed.
    addStubField(uintptr_t(compartment), StubField::Type::RawWord);
  }

  void guardIsExtensible(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardIsExtensible, obj);
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

  Int32OperandId guardAndGetInt32FromString(StringOperandId str) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardAndGetInt32FromString, str);
    writeOperandId(res);
    return res;
  }

  NumberOperandId guardAndGetNumberFromString(StringOperandId str) {
    NumberOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardAndGetNumberFromString, str);
    writeOperandId(res);
    return res;
  }

  NumberOperandId guardAndGetNumberFromBoolean(Int32OperandId boolean) {
    NumberOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardAndGetNumberFromBoolean, boolean);
    writeOperandId(res);
    return res;
  }

  ObjOperandId guardAndGetIterator(ObjOperandId obj,
                                   PropertyIteratorObject* iter,
                                   NativeIterator** enumeratorsAddr) {
    ObjOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::GuardAndGetIterator, obj);
    addStubField(uintptr_t(iter), StubField::Type::JSObject);
    addStubField(uintptr_t(enumeratorsAddr), StubField::Type::RawWord);
    writeOperandId(res);
    return res;
  }

  void guardHasGetterSetter(ObjOperandId obj, Shape* shape) {
    writeOpWithOperandId(CacheOp::GuardHasGetterSetter, obj);
    addStubField(uintptr_t(shape), StubField::Type::Shape);
  }

  void guardGroupHasUnanalyzedNewScript(ObjectGroup* group) {
    writeOp(CacheOp::GuardGroupHasUnanalyzedNewScript);
    addStubField(uintptr_t(group), StubField::Type::ObjectGroup);
  }

  void guardIndexIsNonNegative(Int32OperandId index) {
    writeOpWithOperandId(CacheOp::GuardIndexIsNonNegative, index);
  }

  void guardIndexGreaterThanDenseInitLength(ObjOperandId obj,
                                            Int32OperandId index) {
    writeOpWithOperandId(CacheOp::GuardIndexGreaterThanDenseInitLength, obj);
    writeOperandId(index);
  }

  void guardIndexGreaterThanArrayLength(ObjOperandId obj,
                                        Int32OperandId index) {
    writeOpWithOperandId(CacheOp::GuardIndexGreaterThanArrayLength, obj);
    writeOperandId(index);
  }

  void guardIndexIsValidUpdateOrAdd(ObjOperandId obj, Int32OperandId index) {
    writeOpWithOperandId(CacheOp::GuardIndexIsValidUpdateOrAdd, obj);
    writeOperandId(index);
  }

  void guardTagNotEqual(ValueTagOperandId lhs, ValueTagOperandId rhs) {
    writeOpWithOperandId(CacheOp::GuardTagNotEqual, lhs);
    writeOperandId(rhs);
  }

  void loadFrameCalleeResult() { writeOp(CacheOp::LoadFrameCalleeResult); }
  void loadFrameNumActualArgsResult() {
    writeOp(CacheOp::LoadFrameNumActualArgsResult);
  }

  void loadFrameArgumentResult(Int32OperandId index) {
    writeOpWithOperandId(CacheOp::LoadFrameArgumentResult, index);
  }

  void guardNoDenseElements(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::GuardNoDenseElements, obj);
  }

  ObjOperandId loadObject(JSObject* obj) {
    assertSameCompartment(obj);
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

  ObjOperandId loadWrapperTarget(ObjOperandId obj) {
    ObjOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::LoadWrapperTarget, obj);
    writeOperandId(res);
    return res;
  }

  Int32OperandId truncateDoubleToUInt32(ValOperandId val) {
    Int32OperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::TruncateDoubleToUInt32, val);
    writeOperandId(res);
    return res;
  }

  ValueTagOperandId loadValueTag(ValOperandId val) {
    ValueTagOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::LoadValueTag, val);
    writeOperandId(res);
    return res;
  }

  ValOperandId loadArgumentFixedSlot(
      ArgumentKind kind, uint32_t argc,
      CallFlags flags = CallFlags(CallFlags::Standard)) {
    bool addArgc;
    int32_t slotIndex = GetIndexOfArgument(kind, flags, &addArgc);
    if (addArgc) {
      slotIndex += argc;
    }
    MOZ_ASSERT(slotIndex >= 0);
    MOZ_ASSERT(slotIndex <= UINT8_MAX);

    ValOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::LoadArgumentFixedSlot, res);
    buffer_.writeByte(uint32_t(slotIndex));
    return res;
  }

  ValOperandId loadArgumentDynamicSlot(
      ArgumentKind kind, Int32OperandId argcId,
      CallFlags flags = CallFlags(CallFlags::Standard)) {
    ValOperandId res(nextOperandId_++);

    bool addArgc;
    int32_t slotIndex = GetIndexOfArgument(kind, flags, &addArgc);
    if (addArgc) {
      writeOpWithOperandId(CacheOp::LoadArgumentDynamicSlot, res);
      writeOperandId(argcId);
      buffer_.writeByte(uint32_t(slotIndex));
    } else {
      writeOpWithOperandId(CacheOp::LoadArgumentFixedSlot, res);
      buffer_.writeByte(uint32_t(slotIndex));
    }
    return res;
  }

  void guardFunApply(Int32OperandId argcId, CallFlags flags) {
    writeOpWithOperandId(CacheOp::GuardFunApply, argcId);
    writeCallFlags(flags);
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

  ValOperandId loadDOMExpandoValueGuardGeneration(
      ObjOperandId obj, ExpandoAndGeneration* expandoAndGeneration) {
    ValOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::LoadDOMExpandoValueGuardGeneration, obj);
    addStubField(uintptr_t(expandoAndGeneration), StubField::Type::RawWord);
    addStubField(expandoAndGeneration->generation,
                 StubField::Type::DOMExpandoGeneration);
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

  void addAndStoreFixedSlot(ObjOperandId obj, size_t offset, ValOperandId rhs,
                            Shape* newShape, bool changeGroup,
                            ObjectGroup* newGroup) {
    writeOpWithOperandId(CacheOp::AddAndStoreFixedSlot, obj);
    addStubField(offset, StubField::Type::RawWord);
    writeOperandId(rhs);
    buffer_.writeByte(changeGroup);
    addStubField(uintptr_t(newGroup), StubField::Type::ObjectGroup);
    addStubField(uintptr_t(newShape), StubField::Type::Shape);
  }

  void addAndStoreDynamicSlot(ObjOperandId obj, size_t offset, ValOperandId rhs,
                              Shape* newShape, bool changeGroup,
                              ObjectGroup* newGroup) {
    writeOpWithOperandId(CacheOp::AddAndStoreDynamicSlot, obj);
    addStubField(offset, StubField::Type::RawWord);
    writeOperandId(rhs);
    buffer_.writeByte(changeGroup);
    addStubField(uintptr_t(newGroup), StubField::Type::ObjectGroup);
    addStubField(uintptr_t(newShape), StubField::Type::Shape);
  }

  void allocateAndStoreDynamicSlot(ObjOperandId obj, size_t offset,
                                   ValOperandId rhs, Shape* newShape,
                                   bool changeGroup, ObjectGroup* newGroup,
                                   uint32_t numNewSlots) {
    writeOpWithOperandId(CacheOp::AllocateAndStoreDynamicSlot, obj);
    addStubField(offset, StubField::Type::RawWord);
    writeOperandId(rhs);
    buffer_.writeByte(changeGroup);
    addStubField(uintptr_t(newGroup), StubField::Type::ObjectGroup);
    addStubField(uintptr_t(newShape), StubField::Type::Shape);
    addStubField(numNewSlots, StubField::Type::RawWord);
  }

  void storeTypedObjectReferenceProperty(ObjOperandId obj, uint32_t offset,
                                         TypedThingLayout layout,
                                         ReferenceType type, ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::StoreTypedObjectReferenceProperty, obj);
    addStubField(offset, StubField::Type::RawWord);
    buffer_.writeByte(uint32_t(layout));
    buffer_.writeByte(uint32_t(type));
    writeOperandId(rhs);
  }

  void storeTypedObjectScalarProperty(ObjOperandId obj, uint32_t offset,
                                      TypedThingLayout layout,
                                      Scalar::Type type, OperandId rhs) {
    writeOpWithOperandId(CacheOp::StoreTypedObjectScalarProperty, obj);
    addStubField(offset, StubField::Type::RawWord);
    buffer_.writeByte(uint32_t(layout));
    buffer_.writeByte(uint32_t(type));
    writeOperandId(rhs);
  }

  void storeDenseElement(ObjOperandId obj, Int32OperandId index,
                         ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::StoreDenseElement, obj);
    writeOperandId(index);
    writeOperandId(rhs);
  }

  void storeTypedElement(ObjOperandId obj, TypedThingLayout layout,
                         Scalar::Type elementType, Int32OperandId index,
                         OperandId rhs, bool handleOOB) {
    writeOpWithOperandId(CacheOp::StoreTypedElement, obj);
    buffer_.writeByte(uint32_t(layout));
    buffer_.writeByte(uint32_t(elementType));
    writeOperandId(index);
    writeOperandId(rhs);
    buffer_.writeByte(uint32_t(handleOOB));
  }

  void storeDenseElementHole(ObjOperandId obj, Int32OperandId index,
                             ValOperandId rhs, bool handleAdd) {
    writeOpWithOperandId(CacheOp::StoreDenseElementHole, obj);
    writeOperandId(index);
    writeOperandId(rhs);
    buffer_.writeByte(handleAdd);
  }

  void arrayPush(ObjOperandId obj, ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::ArrayPush, obj);
    writeOperandId(rhs);
  }

  void arrayJoinResult(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::ArrayJoinResult, obj);
  }

  void callScriptedSetter(ObjOperandId obj, JSFunction* setter,
                          ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::CallScriptedSetter, obj);
    addStubField(uintptr_t(setter), StubField::Type::JSObject);
    writeOperandId(rhs);
    buffer_.writeByte(cx_->realm() == setter->realm());
  }

  void callNativeSetter(ObjOperandId obj, JSFunction* setter,
                        ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::CallNativeSetter, obj);
    addStubField(uintptr_t(setter), StubField::Type::JSObject);
    writeOperandId(rhs);
  }

  void callSetArrayLength(ObjOperandId obj, bool strict, ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::CallSetArrayLength, obj);
    buffer_.writeByte(uint32_t(strict));
    writeOperandId(rhs);
  }

  void callProxySet(ObjOperandId obj, jsid id, ValOperandId rhs, bool strict) {
    writeOpWithOperandId(CacheOp::CallProxySet, obj);
    writeOperandId(rhs);
    addStubField(uintptr_t(JSID_BITS(id)), StubField::Type::Id);
    buffer_.writeByte(uint32_t(strict));
  }

  void callProxySetByValue(ObjOperandId obj, ValOperandId id, ValOperandId rhs,
                           bool strict) {
    writeOpWithOperandId(CacheOp::CallProxySetByValue, obj);
    writeOperandId(id);
    writeOperandId(rhs);
    buffer_.writeByte(uint32_t(strict));
  }

  void callAddOrUpdateSparseElementHelper(ObjOperandId obj, Int32OperandId id,
                                          ValOperandId rhs, bool strict) {
    writeOpWithOperandId(CacheOp::CallAddOrUpdateSparseElementHelper, obj);
    writeOperandId(id);
    writeOperandId(rhs);
    buffer_.writeByte(uint32_t(strict));
  }

  StringOperandId callInt32ToString(Int32OperandId id) {
    StringOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::CallInt32ToString, id);
    writeOperandId(res);
    return res;
  }

  StringOperandId callNumberToString(NumberOperandId id) {
    StringOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::CallNumberToString, id);
    writeOperandId(res);
    return res;
  }

  StringOperandId booleanToString(Int32OperandId id) {
    StringOperandId res(nextOperandId_++);
    writeOpWithOperandId(CacheOp::BooleanToString, id);
    writeOperandId(res);
    return res;
  }

  void callScriptedFunction(ObjOperandId calleeId, Int32OperandId argc,
                            CallFlags flags) {
    writeOpWithOperandId(CacheOp::CallScriptedFunction, calleeId);
    writeOperandId(argc);
    writeCallFlags(flags);
  }

  void callNativeFunction(ObjOperandId calleeId, Int32OperandId argc, JSOp op,
                          HandleFunction calleeFunc, CallFlags flags) {
    writeOpWithOperandId(CacheOp::CallNativeFunction, calleeId);
    writeOperandId(argc);
    writeCallFlags(flags);

    // Some native functions can be implemented faster if we know that
    // the return value is ignored.
    bool ignoresReturnValue =
        op == JSOp::CallIgnoresRv && calleeFunc->hasJitInfo() &&
        calleeFunc->jitInfo()->type() == JSJitInfo::IgnoresReturnValueNative;

#ifdef JS_SIMULATOR
    // The simulator requires VM calls to be redirected to a special
    // swi instruction to handle them, so we store the redirected
    // pointer in the stub and use that instead of the original one.
    // If we are calling the ignoresReturnValue version of a native
    // function, we bake it into the redirected pointer.
    // (See BaselineCacheIRCompiler::emitCallNativeFunction.)
    JSNative target = ignoresReturnValue
                          ? calleeFunc->jitInfo()->ignoresReturnValueMethod
                          : calleeFunc->native();
    void* rawPtr = JS_FUNC_TO_DATA_PTR(void*, target);
    void* redirected = Simulator::RedirectNativeFunction(rawPtr, Args_General3);
    addStubField(uintptr_t(redirected), StubField::Type::RawWord);
#else
    // If we are not running in the simulator, we generate different jitcode
    // to find the ignoresReturnValue version of a native function.
    buffer_.writeByte(ignoresReturnValue);
#endif
  }

  void callAnyNativeFunction(ObjOperandId calleeId, Int32OperandId argc,
                             CallFlags flags) {
    MOZ_ASSERT(!flags.isSameRealm());
    writeOpWithOperandId(CacheOp::CallNativeFunction, calleeId);
    writeOperandId(argc);
    writeCallFlags(flags);
#ifdef JS_SIMULATOR
    // The simulator requires native calls to be redirected to a
    // special swi instruction. If we are calling an arbitrary native
    // function, we can't wrap the real target ahead of time, so we
    // call a wrapper function (CallAnyNative) that calls the target
    // itself, and redirect that wrapper.
    JSNative target = CallAnyNative;
    void* rawPtr = JS_FUNC_TO_DATA_PTR(void*, target);
    void* redirected = Simulator::RedirectNativeFunction(rawPtr, Args_General3);
    addStubField(uintptr_t(redirected), StubField::Type::RawWord);
#else
    buffer_.writeByte(/*ignoresReturnValue = */ false);
#endif
  }

  void callClassHook(ObjOperandId calleeId, Int32OperandId argc, JSNative hook,
                     CallFlags flags) {
    writeOpWithOperandId(CacheOp::CallClassHook, calleeId);
    writeOperandId(argc);
    MOZ_ASSERT(!flags.isSameRealm());
    writeCallFlags(flags);
    void* target = JS_FUNC_TO_DATA_PTR(void*, hook);

#ifdef JS_SIMULATOR
    // The simulator requires VM calls to be redirected to a special
    // swi instruction to handle them, so we store the redirected
    // pointer in the stub and use that instead of the original one.
    target = Simulator::RedirectNativeFunction(target, Args_General3);
#endif

    addStubField(uintptr_t(target), StubField::Type::RawWord);
  }

  // These generate no code, but save the template object in a stub
  // field for BaselineInspector.
  void metaNativeTemplateObject(JSObject* templateObject,
                                FieldOffset calleeOffset) {
    writeOp(CacheOp::MetaTwoByte);
    buffer_.writeByte(uint32_t(MetaTwoByteKind::NativeTemplateObject));
    reuseStubField(calleeOffset);
    addStubField(uintptr_t(templateObject), StubField::Type::JSObject);
  }

  void metaScriptedTemplateObject(JSObject* templateObject,
                                  FieldOffset calleeOffset) {
    writeOp(CacheOp::MetaTwoByte);
    buffer_.writeByte(uint32_t(MetaTwoByteKind::ScriptedTemplateObject));
    reuseStubField(calleeOffset);
    addStubField(uintptr_t(templateObject), StubField::Type::JSObject);
  }

  void megamorphicLoadSlotResult(ObjOperandId obj, PropertyName* name,
                                 bool handleMissing) {
    writeOpWithOperandId(CacheOp::MegamorphicLoadSlotResult, obj);
    addStubField(uintptr_t(name), StubField::Type::String);
    buffer_.writeByte(uint32_t(handleMissing));
  }

  void megamorphicLoadSlotByValueResult(ObjOperandId obj, ValOperandId id,
                                        bool handleMissing) {
    writeOpWithOperandId(CacheOp::MegamorphicLoadSlotByValueResult, obj);
    writeOperandId(id);
    buffer_.writeByte(uint32_t(handleMissing));
  }

  void megamorphicStoreSlot(ObjOperandId obj, PropertyName* name,
                            ValOperandId rhs, bool needsTypeBarrier) {
    writeOpWithOperandId(CacheOp::MegamorphicStoreSlot, obj);
    addStubField(uintptr_t(name), StubField::Type::String);
    writeOperandId(rhs);
    buffer_.writeByte(needsTypeBarrier);
  }

  void megamorphicSetElement(ObjOperandId obj, ValOperandId id,
                             ValOperandId rhs, bool strict) {
    writeOpWithOperandId(CacheOp::MegamorphicSetElement, obj);
    writeOperandId(id);
    writeOperandId(rhs);
    buffer_.writeByte(uint32_t(strict));
  }

  void megamorphicHasPropResult(ObjOperandId obj, ValOperandId id,
                                bool hasOwn) {
    writeOpWithOperandId(CacheOp::MegamorphicHasPropResult, obj);
    writeOperandId(id);
    buffer_.writeByte(uint32_t(hasOwn));
  }

  void doubleAddResult(NumberOperandId lhsId, NumberOperandId rhsId) {
    writeOpWithOperandId(CacheOp::DoubleAddResult, lhsId);
    writeOperandId(rhsId);
  }

  void doubleSubResult(NumberOperandId lhsId, NumberOperandId rhsId) {
    writeOpWithOperandId(CacheOp::DoubleSubResult, lhsId);
    writeOperandId(rhsId);
  }

  void doubleMulResult(NumberOperandId lhsId, NumberOperandId rhsId) {
    writeOpWithOperandId(CacheOp::DoubleMulResult, lhsId);
    writeOperandId(rhsId);
  }

  void doubleDivResult(NumberOperandId lhsId, NumberOperandId rhsId) {
    writeOpWithOperandId(CacheOp::DoubleDivResult, lhsId);
    writeOperandId(rhsId);
  }

  void doubleModResult(NumberOperandId lhsId, NumberOperandId rhsId) {
    writeOpWithOperandId(CacheOp::DoubleModResult, lhsId);
    writeOperandId(rhsId);
  }

  void doublePowResult(NumberOperandId lhsId, NumberOperandId rhsId) {
    writeOpWithOperandId(CacheOp::DoublePowResult, lhsId);
    writeOperandId(rhsId);
  }

  void int32AddResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32AddResult, lhs);
    writeOperandId(rhs);
  }

  void int32SubResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32SubResult, lhs);
    writeOperandId(rhs);
  }

  void int32MulResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32MulResult, lhs);
    writeOperandId(rhs);
  }

  void int32DivResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32DivResult, lhs);
    writeOperandId(rhs);
  }

  void int32ModResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32ModResult, lhs);
    writeOperandId(rhs);
  }

  void int32PowResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32PowResult, lhs);
    writeOperandId(rhs);
  }

  void bigIntAddResult(BigIntOperandId lhsId, BigIntOperandId rhsId) {
    writeOpWithOperandId(CacheOp::BigIntAddResult, lhsId);
    writeOperandId(rhsId);
  }

  void bigIntSubResult(BigIntOperandId lhsId, BigIntOperandId rhsId) {
    writeOpWithOperandId(CacheOp::BigIntSubResult, lhsId);
    writeOperandId(rhsId);
  }

  void bigIntMulResult(BigIntOperandId lhsId, BigIntOperandId rhsId) {
    writeOpWithOperandId(CacheOp::BigIntMulResult, lhsId);
    writeOperandId(rhsId);
  }

  void bigIntDivResult(BigIntOperandId lhsId, BigIntOperandId rhsId) {
    writeOpWithOperandId(CacheOp::BigIntDivResult, lhsId);
    writeOperandId(rhsId);
  }

  void bigIntModResult(BigIntOperandId lhsId, BigIntOperandId rhsId) {
    writeOpWithOperandId(CacheOp::BigIntModResult, lhsId);
    writeOperandId(rhsId);
  }

  void bigIntPowResult(BigIntOperandId lhsId, BigIntOperandId rhsId) {
    writeOpWithOperandId(CacheOp::BigIntPowResult, lhsId);
    writeOperandId(rhsId);
  }

  void int32BitOrResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32BitOrResult, lhs);
    writeOperandId(rhs);
  }

  void int32BitXOrResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32BitXorResult, lhs);
    writeOperandId(rhs);
  }

  void int32BitAndResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32BitAndResult, lhs);
    writeOperandId(rhs);
  }

  void int32LeftShiftResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32LeftShiftResult, lhs);
    writeOperandId(rhs);
  }

  void int32RightShiftResult(Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::Int32RightShiftResult, lhs);
    writeOperandId(rhs);
  }

  void int32URightShiftResult(Int32OperandId lhs, Int32OperandId rhs,
                              bool allowDouble) {
    writeOpWithOperandId(CacheOp::Int32URightShiftResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint32_t(allowDouble));
  }

  void int32NotResult(Int32OperandId id) {
    writeOpWithOperandId(CacheOp::Int32NotResult, id);
  }

  void int32NegationResult(Int32OperandId id) {
    writeOpWithOperandId(CacheOp::Int32NegationResult, id);
  }

  void int32IncResult(Int32OperandId id) {
    writeOpWithOperandId(CacheOp::Int32IncResult, id);
  }

  void int32DecResult(Int32OperandId id) {
    writeOpWithOperandId(CacheOp::Int32DecResult, id);
  }

  void doubleNegationResult(NumberOperandId val) {
    writeOpWithOperandId(CacheOp::DoubleNegationResult, val);
  }

  void doubleIncResult(NumberOperandId val) {
    writeOpWithOperandId(CacheOp::DoubleIncResult, val);
  }

  void doubleDecResult(NumberOperandId val) {
    writeOpWithOperandId(CacheOp::DoubleDecResult, val);
  }

  void bigIntBitOrResult(BigIntOperandId lhs, BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::BigIntBitOrResult, lhs);
    writeOperandId(rhs);
  }

  void bigIntBitXorResult(BigIntOperandId lhs, BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::BigIntBitXorResult, lhs);
    writeOperandId(rhs);
  }

  void bigIntBitAndResult(BigIntOperandId lhs, BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::BigIntBitAndResult, lhs);
    writeOperandId(rhs);
  }

  void bigIntLeftShiftResult(BigIntOperandId lhs, BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::BigIntLeftShiftResult, lhs);
    writeOperandId(rhs);
  }

  void bigIntRightShiftResult(BigIntOperandId lhs, BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::BigIntRightShiftResult, lhs);
    writeOperandId(rhs);
  }

  void bigIntNotResult(BigIntOperandId id) {
    writeOpWithOperandId(CacheOp::BigIntNotResult, id);
  }

  void bigIntNegationResult(BigIntOperandId id) {
    writeOpWithOperandId(CacheOp::BigIntNegationResult, id);
  }

  void bigIntIncResult(BigIntOperandId id) {
    writeOpWithOperandId(CacheOp::BigIntIncResult, id);
  }

  void bigIntDecResult(BigIntOperandId id) {
    writeOpWithOperandId(CacheOp::BigIntDecResult, id);
  }

  void loadBooleanResult(bool val) {
    writeOp(CacheOp::LoadBooleanResult);
    buffer_.writeByte(uint32_t(val));
  }

  void loadUndefinedResult() { writeOp(CacheOp::LoadUndefinedResult); }
  void loadStringResult(JSString* str) {
    writeOp(CacheOp::LoadStringResult);
    addStubField(uintptr_t(str), StubField::Type::String);
  }

  void loadFixedSlotResult(ObjOperandId obj, size_t offset) {
    writeOpWithOperandId(CacheOp::LoadFixedSlotResult, obj);
    addStubField(offset, StubField::Type::RawWord);
  }

  void loadDynamicSlotResult(ObjOperandId obj, size_t offset) {
    writeOpWithOperandId(CacheOp::LoadDynamicSlotResult, obj);
    addStubField(offset, StubField::Type::RawWord);
  }

  void loadTypedObjectResult(ObjOperandId obj, uint32_t offset,
                             TypedThingLayout layout, uint32_t typeDescr) {
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

  void loadArgumentsObjectLengthResult(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::LoadArgumentsObjectLengthResult, obj);
  }

  void loadFunctionLengthResult(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::LoadFunctionLengthResult, obj);
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

  void callGetSparseElementResult(ObjOperandId obj, Int32OperandId index) {
    writeOpWithOperandId(CacheOp::CallGetSparseElementResult, obj);
    writeOperandId(index);
  }

  void loadDenseElementExistsResult(ObjOperandId obj, Int32OperandId index) {
    writeOpWithOperandId(CacheOp::LoadDenseElementExistsResult, obj);
    writeOperandId(index);
  }

  void loadTypedElementExistsResult(ObjOperandId obj, Int32OperandId index,
                                    TypedThingLayout layout) {
    writeOpWithOperandId(CacheOp::LoadTypedElementExistsResult, obj);
    writeOperandId(index);
    buffer_.writeByte(uint32_t(layout));
  }

  void loadDenseElementHoleExistsResult(ObjOperandId obj,
                                        Int32OperandId index) {
    writeOpWithOperandId(CacheOp::LoadDenseElementHoleExistsResult, obj);
    writeOperandId(index);
  }

  void loadTypedElementResult(ObjOperandId obj, Int32OperandId index,
                              TypedThingLayout layout, Scalar::Type elementType,
                              bool handleOOB) {
    writeOpWithOperandId(CacheOp::LoadTypedElementResult, obj);
    writeOperandId(index);
    buffer_.writeByte(uint32_t(layout));
    buffer_.writeByte(uint32_t(elementType));
    buffer_.writeByte(uint32_t(handleOOB));
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
    buffer_.writeByte(cx_->realm() == getter->realm());
  }

  void callScriptedGetterByValueResult(ValOperandId obj, JSFunction* getter) {
    writeOpWithOperandId(CacheOp::CallScriptedGetterByValueResult, obj);
    addStubField(uintptr_t(getter), StubField::Type::JSObject);
    buffer_.writeByte(cx_->realm() == getter->realm());
  }

  void callNativeGetterResult(ObjOperandId obj, JSFunction* getter) {
    writeOpWithOperandId(CacheOp::CallNativeGetterResult, obj);
    addStubField(uintptr_t(getter), StubField::Type::JSObject);
  }

  void callNativeGetterByValueResult(ValOperandId obj, JSFunction* getter) {
    writeOpWithOperandId(CacheOp::CallNativeGetterByValueResult, obj);
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

  void callProxyHasPropResult(ObjOperandId obj, ValOperandId idVal,
                              bool hasOwn) {
    writeOpWithOperandId(CacheOp::CallProxyHasPropResult, obj);
    writeOperandId(idVal);
    buffer_.writeByte(uint32_t(hasOwn));
  }

  void callObjectHasSparseElementResult(ObjOperandId obj,
                                        Int32OperandId index) {
    writeOpWithOperandId(CacheOp::CallObjectHasSparseElementResult, obj);
    writeOperandId(index);
  }

  void callNativeGetElementResult(ObjOperandId obj, Int32OperandId index) {
    writeOpWithOperandId(CacheOp::CallNativeGetElementResult, obj);
    writeOperandId(index);
  }

  void callIsSuspendedGeneratorResult(ValOperandId val) {
    writeOpWithOperandId(CacheOp::CallIsSuspendedGeneratorResult, val);
  }

  void loadEnvironmentFixedSlotResult(ObjOperandId obj, size_t offset) {
    writeOpWithOperandId(CacheOp::LoadEnvironmentFixedSlotResult, obj);
    addStubField(offset, StubField::Type::RawWord);
  }

  void loadEnvironmentDynamicSlotResult(ObjOperandId obj, size_t offset) {
    writeOpWithOperandId(CacheOp::LoadEnvironmentDynamicSlotResult, obj);
    addStubField(offset, StubField::Type::RawWord);
  }

  void loadObjectResult(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::LoadObjectResult, obj);
  }

  void loadInt32Result(Int32OperandId val) {
    writeOpWithOperandId(CacheOp::LoadInt32Result, val);
  }

  void loadDoubleResult(NumberOperandId val) {
    writeOpWithOperandId(CacheOp::LoadDoubleResult, val);
  }

  void loadInstanceOfObjectResult(ValOperandId lhs, ObjOperandId protoId,
                                  uint32_t slot) {
    writeOpWithOperandId(CacheOp::LoadInstanceOfObjectResult, lhs);
    writeOperandId(protoId);
  }

  void loadTypeOfObjectResult(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::LoadTypeOfObjectResult, obj);
  }

  void loadInt32TruthyResult(ValOperandId integer) {
    writeOpWithOperandId(CacheOp::LoadInt32TruthyResult, integer);
  }

  void loadDoubleTruthyResult(ValOperandId dbl) {
    writeOpWithOperandId(CacheOp::LoadDoubleTruthyResult, dbl);
  }

  void loadStringTruthyResult(StringOperandId str) {
    writeOpWithOperandId(CacheOp::LoadStringTruthyResult, str);
  }

  void loadObjectTruthyResult(ObjOperandId obj) {
    writeOpWithOperandId(CacheOp::LoadObjectTruthyResult, obj);
  }

  void loadBigIntTruthyResult(BigIntOperandId obj) {
    writeOpWithOperandId(CacheOp::LoadBigIntTruthyResult, obj);
  }

  void loadValueResult(const Value& val) {
    writeOp(CacheOp::LoadValueResult);
    addStubField(val.asRawBits(), StubField::Type::Value);
  }

  void loadNewObjectFromTemplateResult(JSObject* templateObj) {
    writeOp(CacheOp::LoadNewObjectFromTemplateResult);
    addStubField(uintptr_t(templateObj), StubField::Type::JSObject);
    // Bake in a monotonically increasing number to ensure we differentiate
    // between different baseline stubs that otherwise might share
    // stub code.
    uint64_t id = cx_->runtime()->jitRuntime()->nextDisambiguationId();
    writeUint32Immediate(id & UINT32_MAX);
    writeUint32Immediate(id >> 32);
  }

  void callStringConcatResult(StringOperandId lhs, StringOperandId rhs) {
    writeOpWithOperandId(CacheOp::CallStringConcatResult, lhs);
    writeOperandId(rhs);
  }

  void callStringObjectConcatResult(ValOperandId lhs, ValOperandId rhs) {
    writeOpWithOperandId(CacheOp::CallStringObjectConcatResult, lhs);
    writeOperandId(rhs);
  }

  void compareStringResult(JSOp op, StringOperandId lhs, StringOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareStringResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareObjectResult(JSOp op, ObjOperandId lhs, ObjOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareObjectResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareObjectUndefinedNullResult(JSOp op, ObjOperandId object) {
    writeOpWithOperandId(CacheOp::CompareObjectUndefinedNullResult, object);
    buffer_.writeByte(uint8_t(op));
  }

  void compareSymbolResult(JSOp op, SymbolOperandId lhs, SymbolOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareSymbolResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareInt32Result(JSOp op, Int32OperandId lhs, Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareInt32Result, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareDoubleResult(JSOp op, NumberOperandId lhs, NumberOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareDoubleResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareBigIntResult(JSOp op, BigIntOperandId lhs, BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareBigIntResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareBigIntInt32Result(JSOp op, BigIntOperandId lhs,
                                Int32OperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareBigIntInt32Result, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareInt32BigIntResult(JSOp op, Int32OperandId lhs,
                                BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareInt32BigIntResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareBigIntNumberResult(JSOp op, BigIntOperandId lhs,
                                 NumberOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareBigIntNumberResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareNumberBigIntResult(JSOp op, NumberOperandId lhs,
                                 BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareNumberBigIntResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareBigIntStringResult(JSOp op, BigIntOperandId lhs,
                                 StringOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareBigIntStringResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void compareStringBigIntResult(JSOp op, StringOperandId lhs,
                                 BigIntOperandId rhs) {
    writeOpWithOperandId(CacheOp::CompareStringBigIntResult, lhs);
    writeOperandId(rhs);
    buffer_.writeByte(uint8_t(op));
  }

  void callPrintString(const char* str) {
    writeOp(CacheOp::CallPrintString);
    writePointer(const_cast<char*>(str));
  }

  void breakpoint() { writeOp(CacheOp::Breakpoint); }
  void typeMonitorResult() { writeOp(CacheOp::TypeMonitorResult); }
  void returnFromIC() { writeOp(CacheOp::ReturnFromIC); }
  void wrapResult() { writeOp(CacheOp::WrapResult); }
};

class CacheIRStubInfo;

// Helper class for reading CacheIR bytecode.
class MOZ_RAII CacheIRReader {
  CompactBufferReader buffer_;

  CacheIRReader(const CacheIRReader&) = delete;
  CacheIRReader& operator=(const CacheIRReader&) = delete;

 public:
  CacheIRReader(const uint8_t* start, const uint8_t* end)
      : buffer_(start, end) {}
  explicit CacheIRReader(const CacheIRWriter& writer)
      : CacheIRReader(writer.codeStart(), writer.codeEnd()) {}
  explicit CacheIRReader(const CacheIRStubInfo* stubInfo);

  bool more() const { return buffer_.more(); }

  CacheOp readOp() { return CacheOp(buffer_.readByte()); }

  // Skip data not currently used.
  void skip() { buffer_.readByte(); }
  void skip(uint32_t skipLength) {
    if (skipLength > 0) {
      buffer_.seek(buffer_.currentPosition(), skipLength);
    }
  }

  ValOperandId valOperandId() { return ValOperandId(buffer_.readByte()); }
  ValueTagOperandId valueTagOperandId() {
    return ValueTagOperandId(buffer_.readByte());
  }

  ObjOperandId objOperandId() { return ObjOperandId(buffer_.readByte()); }
  NumberOperandId numberOperandId() {
    return NumberOperandId(buffer_.readByte());
  }
  StringOperandId stringOperandId() {
    return StringOperandId(buffer_.readByte());
  }

  SymbolOperandId symbolOperandId() {
    return SymbolOperandId(buffer_.readByte());
  }

  BigIntOperandId bigIntOperandId() {
    return BigIntOperandId(buffer_.readByte());
  }

  Int32OperandId int32OperandId() { return Int32OperandId(buffer_.readByte()); }

  uint32_t stubOffset() { return buffer_.readByte() * sizeof(uintptr_t); }
  GuardClassKind guardClassKind() { return GuardClassKind(buffer_.readByte()); }
  JSValueType jsValueType() { return JSValueType(buffer_.readByte()); }
  ValueType valueType() { return ValueType(buffer_.readByte()); }

  TypedThingLayout typedThingLayout() {
    return TypedThingLayout(buffer_.readByte());
  }

  Scalar::Type scalarType() { return Scalar::Type(buffer_.readByte()); }
  uint32_t typeDescrKey() { return buffer_.readByte(); }
  JSWhyMagic whyMagic() { return JSWhyMagic(buffer_.readByte()); }
  JSOp jsop() { return JSOp(buffer_.readByte()); }
  int32_t int32Immediate() { return int32_t(buffer_.readFixedUint32_t()); }
  uint32_t uint32Immediate() { return buffer_.readFixedUint32_t(); }
  void* pointer() { return buffer_.readRawPointer(); }

  template <typename MetaKind>
  MetaKind metaKind() {
    return MetaKind(buffer_.readByte());
  }

  ReferenceType referenceTypeDescrType() {
    return ReferenceType(buffer_.readByte());
  }

  CallFlags callFlags() {
    // See CacheIRWriter::writeCallFlags()
    uint8_t encoded = buffer_.readByte();
    CallFlags::ArgFormat format =
        CallFlags::ArgFormat(encoded & CallFlags::ArgFormatMask);
    bool isConstructing = encoded & CallFlags::IsConstructing;
    bool isSameRealm = encoded & CallFlags::IsSameRealm;
    switch (format) {
      case CallFlags::Standard:
        return CallFlags(isConstructing, /*isSpread =*/false, isSameRealm);
      case CallFlags::Spread:
        return CallFlags(isConstructing, /*isSpread =*/true, isSameRealm);
      default:
        // The existing non-standard argument formats (FunCall and FunApply)
        // can't be constructors and have no support for isSameRealm.
        MOZ_ASSERT(!isConstructing && !isSameRealm);
        return CallFlags(format);
    }
  }

  uint8_t readByte() { return buffer_.readByte(); }
  bool readBool() {
    uint8_t b = buffer_.readByte();
    MOZ_ASSERT(b <= 1);
    return bool(b);
  }

  bool matchOp(CacheOp op) {
    const uint8_t* pos = buffer_.currentPosition();
    if (readOp() == op) {
      return true;
    }
    buffer_.seek(pos, 0);
    return false;
  }

  bool matchOp(CacheOp op, OperandId id) {
    const uint8_t* pos = buffer_.currentPosition();
    if (readOp() == op && buffer_.readByte() == id.id()) {
      return true;
    }
    buffer_.seek(pos, 0);
    return false;
  }

  bool matchOpEither(CacheOp op1, CacheOp op2) {
    const uint8_t* pos = buffer_.currentPosition();
    CacheOp op = readOp();
    if (op == op1 || op == op2) {
      return true;
    }
    buffer_.seek(pos, 0);
    return false;
  }
  const uint8_t* currentPosition() const { return buffer_.currentPosition(); }
};

class MOZ_RAII IRGenerator {
 protected:
  CacheIRWriter writer;
  JSContext* cx_;
  HandleScript script_;
  jsbytecode* pc_;
  CacheKind cacheKind_;
  ICState::Mode mode_;

  IRGenerator(const IRGenerator&) = delete;
  IRGenerator& operator=(const IRGenerator&) = delete;

  bool maybeGuardInt32Index(const Value& index, ValOperandId indexId,
                            uint32_t* int32Index, Int32OperandId* int32IndexId);

  ObjOperandId guardDOMProxyExpandoObjectAndShape(JSObject* obj,
                                                  ObjOperandId objId,
                                                  const Value& expandoVal,
                                                  JSObject* expandoObj);

  void emitIdGuard(ValOperandId valId, jsid id);

  friend class CacheIRSpewer;

 public:
  explicit IRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                       CacheKind cacheKind, ICState::Mode mode);

  const CacheIRWriter& writerRef() const { return writer; }
  CacheKind cacheKind() const { return cacheKind_; }

  static constexpr char* NotAttached = nullptr;
};

// Flags used to describe what values a GetProperty cache may produce.
enum class GetPropertyResultFlags {
  None = 0,

  // Values produced by this cache will go through a type barrier,
  // so the cache may produce any type of value that is compatible with its
  // result operand.
  Monitored = 1 << 0,

  // Whether particular primitives may be produced by this cache.
  AllowUndefined = 1 << 1,
  AllowInt32 = 1 << 2,
  AllowDouble = 1 << 3,

  All = Monitored | AllowUndefined | AllowInt32 | AllowDouble
};

static inline bool operator&(GetPropertyResultFlags a,
                             GetPropertyResultFlags b) {
  return static_cast<int>(a) & static_cast<int>(b);
}

static inline GetPropertyResultFlags operator|(GetPropertyResultFlags a,
                                               GetPropertyResultFlags b) {
  return static_cast<GetPropertyResultFlags>(static_cast<int>(a) |
                                             static_cast<int>(b));
}

static inline GetPropertyResultFlags& operator|=(GetPropertyResultFlags& lhs,
                                                 GetPropertyResultFlags b) {
  lhs = lhs | b;
  return lhs;
}

// GetPropIRGenerator generates CacheIR for a GetProp IC.
class MOZ_RAII GetPropIRGenerator : public IRGenerator {
  HandleValue val_;
  HandleValue idVal_;
  HandleValue receiver_;
  GetPropertyResultFlags resultFlags_;

  enum class PreliminaryObjectAction { None, Unlink, NotePreliminary };
  PreliminaryObjectAction preliminaryObjectAction_;

  AttachDecision tryAttachNative(HandleObject obj, ObjOperandId objId,
                                 HandleId id);
  AttachDecision tryAttachUnboxed(HandleObject obj, ObjOperandId objId,
                                  HandleId id);
  AttachDecision tryAttachUnboxedExpando(HandleObject obj, ObjOperandId objId,
                                         HandleId id);
  AttachDecision tryAttachTypedObject(HandleObject obj, ObjOperandId objId,
                                      HandleId id);
  AttachDecision tryAttachObjectLength(HandleObject obj, ObjOperandId objId,
                                       HandleId id);
  AttachDecision tryAttachModuleNamespace(HandleObject obj, ObjOperandId objId,
                                          HandleId id);
  AttachDecision tryAttachWindowProxy(HandleObject obj, ObjOperandId objId,
                                      HandleId id);
  AttachDecision tryAttachCrossCompartmentWrapper(HandleObject obj,
                                                  ObjOperandId objId,
                                                  HandleId id);
  AttachDecision tryAttachXrayCrossCompartmentWrapper(HandleObject obj,
                                                      ObjOperandId objId,
                                                      HandleId id);
  AttachDecision tryAttachFunction(HandleObject obj, ObjOperandId objId,
                                   HandleId id);

  AttachDecision tryAttachGenericProxy(HandleObject obj, ObjOperandId objId,
                                       HandleId id, bool handleDOMProxies);
  AttachDecision tryAttachDOMProxyExpando(HandleObject obj, ObjOperandId objId,
                                          HandleId id);
  AttachDecision tryAttachDOMProxyShadowed(HandleObject obj, ObjOperandId objId,
                                           HandleId id);
  AttachDecision tryAttachDOMProxyUnshadowed(HandleObject obj,
                                             ObjOperandId objId, HandleId id);
  AttachDecision tryAttachProxy(HandleObject obj, ObjOperandId objId,
                                HandleId id);

  AttachDecision tryAttachPrimitive(ValOperandId valId, HandleId id);
  AttachDecision tryAttachStringChar(ValOperandId valId, ValOperandId indexId);
  AttachDecision tryAttachStringLength(ValOperandId valId, HandleId id);
  AttachDecision tryAttachMagicArgumentsName(ValOperandId valId, HandleId id);

  AttachDecision tryAttachMagicArgument(ValOperandId valId,
                                        ValOperandId indexId);
  AttachDecision tryAttachArgumentsObjectArg(HandleObject obj,
                                             ObjOperandId objId,
                                             Int32OperandId indexId);

  AttachDecision tryAttachDenseElement(HandleObject obj, ObjOperandId objId,
                                       uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachDenseElementHole(HandleObject obj, ObjOperandId objId,
                                           uint32_t index,
                                           Int32OperandId indexId);
  AttachDecision tryAttachSparseElement(HandleObject obj, ObjOperandId objId,
                                        uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachTypedElement(HandleObject obj, ObjOperandId objId,
                                       uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachTypedArrayNonInt32Index(HandleObject obj,
                                                  ObjOperandId objId);

  AttachDecision tryAttachGenericElement(HandleObject obj, ObjOperandId objId,
                                         uint32_t index,
                                         Int32OperandId indexId);

  AttachDecision tryAttachProxyElement(HandleObject obj, ObjOperandId objId);

  void attachMegamorphicNativeSlot(ObjOperandId objId, jsid id,
                                   bool handleMissing);

  ValOperandId getElemKeyValueId() const {
    MOZ_ASSERT(cacheKind_ == CacheKind::GetElem ||
               cacheKind_ == CacheKind::GetElemSuper);
    return ValOperandId(1);
  }

  ValOperandId getSuperReceiverValueId() const {
    if (cacheKind_ == CacheKind::GetPropSuper) {
      return ValOperandId(1);
    }

    MOZ_ASSERT(cacheKind_ == CacheKind::GetElemSuper);
    return ValOperandId(2);
  }

  bool isSuper() const {
    return (cacheKind_ == CacheKind::GetPropSuper ||
            cacheKind_ == CacheKind::GetElemSuper);
  }

  // No pc if idempotent, as there can be multiple bytecode locations
  // due to GVN.
  bool idempotent() const { return pc_ == nullptr; }

  // If this is a GetElem cache, emit instructions to guard the incoming Value
  // matches |id|.
  void maybeEmitIdGuard(jsid id);

  void trackAttached(const char* name);

 public:
  GetPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     ICState::Mode mode, CacheKind cacheKind, HandleValue val,
                     HandleValue idVal, HandleValue receiver,
                     GetPropertyResultFlags resultFlags);

  AttachDecision tryAttachStub();
  AttachDecision tryAttachIdempotentStub();

  bool shouldUnlinkPreliminaryObjectStubs() const {
    return preliminaryObjectAction_ == PreliminaryObjectAction::Unlink;
  }

  bool shouldNotePreliminaryObjectStub() const {
    return preliminaryObjectAction_ == PreliminaryObjectAction::NotePreliminary;
  }
};

// GetNameIRGenerator generates CacheIR for a GetName IC.
class MOZ_RAII GetNameIRGenerator : public IRGenerator {
  HandleObject env_;
  HandlePropertyName name_;

  AttachDecision tryAttachGlobalNameValue(ObjOperandId objId, HandleId id);
  AttachDecision tryAttachGlobalNameGetter(ObjOperandId objId, HandleId id);
  AttachDecision tryAttachEnvironmentName(ObjOperandId objId, HandleId id);

  void trackAttached(const char* name);

 public:
  GetNameIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     ICState::Mode mode, HandleObject env,
                     HandlePropertyName name);

  AttachDecision tryAttachStub();
};

// BindNameIRGenerator generates CacheIR for a BindName IC.
class MOZ_RAII BindNameIRGenerator : public IRGenerator {
  HandleObject env_;
  HandlePropertyName name_;

  AttachDecision tryAttachGlobalName(ObjOperandId objId, HandleId id);
  AttachDecision tryAttachEnvironmentName(ObjOperandId objId, HandleId id);

  void trackAttached(const char* name);

 public:
  BindNameIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                      ICState::Mode mode, HandleObject env,
                      HandlePropertyName name);

  AttachDecision tryAttachStub();
};

// Information used by SetProp/SetElem stubs to check/update property types.
class MOZ_RAII PropertyTypeCheckInfo {
  RootedObjectGroup group_;
  RootedId id_;
  bool needsTypeBarrier_;

  PropertyTypeCheckInfo(const PropertyTypeCheckInfo&) = delete;
  void operator=(const PropertyTypeCheckInfo&) = delete;

 public:
  PropertyTypeCheckInfo(JSContext* cx, bool needsTypeBarrier)
      : group_(cx), id_(cx), needsTypeBarrier_(needsTypeBarrier) {
    if (!IsTypeInferenceEnabled()) {
      needsTypeBarrier_ = false;
    }
  }

  bool needsTypeBarrier() const { return needsTypeBarrier_; }
  bool isSet() const { return group_ != nullptr; }

  ObjectGroup* group() const {
    MOZ_ASSERT(isSet());
    return group_;
  }

  jsid id() const {
    MOZ_ASSERT(isSet());
    return id_;
  }

  void set(ObjectGroup* group, jsid id) {
    MOZ_ASSERT(!group_);
    MOZ_ASSERT(group);
    if (needsTypeBarrier_) {
      group_ = group;
      id_ = id;
    }
  }
};

// SetPropIRGenerator generates CacheIR for a SetProp IC.
class MOZ_RAII SetPropIRGenerator : public IRGenerator {
  HandleValue lhsVal_;
  HandleValue idVal_;
  HandleValue rhsVal_;
  PropertyTypeCheckInfo typeCheckInfo_;

  enum class PreliminaryObjectAction { None, Unlink, NotePreliminary };
  PreliminaryObjectAction preliminaryObjectAction_;
  bool attachedTypedArrayOOBStub_;

  bool maybeHasExtraIndexedProps_;

 public:
  enum class DeferType { None, AddSlot };

 private:
  DeferType deferType_ = DeferType::None;

  ValOperandId setElemKeyValueId() const {
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    return ValOperandId(1);
  }

  ValOperandId rhsValueId() const {
    if (cacheKind_ == CacheKind::SetProp) {
      return ValOperandId(1);
    }
    MOZ_ASSERT(cacheKind_ == CacheKind::SetElem);
    return ValOperandId(2);
  }

  // If this is a SetElem cache, emit instructions to guard the incoming Value
  // matches |id|.
  void maybeEmitIdGuard(jsid id);

  OperandId emitNumericGuard(ValOperandId valId, Scalar::Type type);

  AttachDecision tryAttachNativeSetSlot(HandleObject obj, ObjOperandId objId,
                                        HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachUnboxedExpandoSetSlot(HandleObject obj,
                                                ObjOperandId objId, HandleId id,
                                                ValOperandId rhsId);
  AttachDecision tryAttachUnboxedProperty(HandleObject obj, ObjOperandId objId,
                                          HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachTypedObjectProperty(HandleObject obj,
                                              ObjOperandId objId, HandleId id,
                                              ValOperandId rhsId);
  AttachDecision tryAttachSetter(HandleObject obj, ObjOperandId objId,
                                 HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachSetArrayLength(HandleObject obj, ObjOperandId objId,
                                         HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachWindowProxy(HandleObject obj, ObjOperandId objId,
                                      HandleId id, ValOperandId rhsId);

  AttachDecision tryAttachSetDenseElement(HandleObject obj, ObjOperandId objId,
                                          uint32_t index,
                                          Int32OperandId indexId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachSetTypedElement(HandleObject obj, ObjOperandId objId,
                                          uint32_t index,
                                          Int32OperandId indexId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachSetTypedArrayElementNonInt32Index(HandleObject obj,
                                                            ObjOperandId objId,
                                                            ValOperandId rhsId);

  AttachDecision tryAttachSetDenseElementHole(HandleObject obj,
                                              ObjOperandId objId,
                                              uint32_t index,
                                              Int32OperandId indexId,
                                              ValOperandId rhsId);

  AttachDecision tryAttachAddOrUpdateSparseElement(HandleObject obj,
                                                   ObjOperandId objId,
                                                   uint32_t index,
                                                   Int32OperandId indexId,
                                                   ValOperandId rhsId);

  AttachDecision tryAttachGenericProxy(HandleObject obj, ObjOperandId objId,
                                       HandleId id, ValOperandId rhsId,
                                       bool handleDOMProxies);
  AttachDecision tryAttachDOMProxyShadowed(HandleObject obj, ObjOperandId objId,
                                           HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachDOMProxyUnshadowed(HandleObject obj,
                                             ObjOperandId objId, HandleId id,
                                             ValOperandId rhsId);
  AttachDecision tryAttachDOMProxyExpando(HandleObject obj, ObjOperandId objId,
                                          HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachProxy(HandleObject obj, ObjOperandId objId,
                                HandleId id, ValOperandId rhsId);
  AttachDecision tryAttachProxyElement(HandleObject obj, ObjOperandId objId,
                                       ValOperandId rhsId);
  AttachDecision tryAttachMegamorphicSetElement(HandleObject obj,
                                                ObjOperandId objId,
                                                ValOperandId rhsId);

  bool canAttachAddSlotStub(HandleObject obj, HandleId id);

 public:
  SetPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     CacheKind cacheKind, ICState::Mode mode,
                     HandleValue lhsVal, HandleValue idVal, HandleValue rhsVal,
                     bool needsTypeBarrier = true,
                     bool maybeHasExtraIndexedProps = true);

  AttachDecision tryAttachStub();
  AttachDecision tryAttachAddSlotStub(HandleObjectGroup oldGroup,
                                      HandleShape oldShape);
  void trackAttached(const char* name);

  bool shouldUnlinkPreliminaryObjectStubs() const {
    return preliminaryObjectAction_ == PreliminaryObjectAction::Unlink;
  }

  bool shouldNotePreliminaryObjectStub() const {
    return preliminaryObjectAction_ == PreliminaryObjectAction::NotePreliminary;
  }

  const PropertyTypeCheckInfo* typeCheckInfo() const { return &typeCheckInfo_; }

  bool attachedTypedArrayOOBStub() const { return attachedTypedArrayOOBStub_; }

  DeferType deferType() const { return deferType_; }
};

// HasPropIRGenerator generates CacheIR for a HasProp IC. Used for
// CacheKind::In / CacheKind::HasOwn.
class MOZ_RAII HasPropIRGenerator : public IRGenerator {
  HandleValue val_;
  HandleValue idVal_;

  AttachDecision tryAttachDense(HandleObject obj, ObjOperandId objId,
                                uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachDenseHole(HandleObject obj, ObjOperandId objId,
                                    uint32_t index, Int32OperandId indexId);
  AttachDecision tryAttachTypedArray(HandleObject obj, ObjOperandId objId,
                                     Int32OperandId indexId);
  AttachDecision tryAttachTypedArrayNonInt32Index(HandleObject obj,
                                                  ObjOperandId objId,
                                                  ValOperandId keyId);
  AttachDecision tryAttachSparse(HandleObject obj, ObjOperandId objId,
                                 Int32OperandId indexId);
  AttachDecision tryAttachNamedProp(HandleObject obj, ObjOperandId objId,
                                    HandleId key, ValOperandId keyId);
  AttachDecision tryAttachMegamorphic(ObjOperandId objId, ValOperandId keyId);
  AttachDecision tryAttachNative(JSObject* obj, ObjOperandId objId, jsid key,
                                 ValOperandId keyId, PropertyResult prop,
                                 JSObject* holder);
  AttachDecision tryAttachUnboxed(JSObject* obj, ObjOperandId objId, jsid key,
                                  ValOperandId keyId);
  AttachDecision tryAttachUnboxedExpando(JSObject* obj, ObjOperandId objId,
                                         jsid key, ValOperandId keyId);
  AttachDecision tryAttachTypedObject(JSObject* obj, ObjOperandId objId,
                                      jsid key, ValOperandId keyId);
  AttachDecision tryAttachSlotDoesNotExist(JSObject* obj, ObjOperandId objId,
                                           jsid key, ValOperandId keyId);
  AttachDecision tryAttachDoesNotExist(HandleObject obj, ObjOperandId objId,
                                       HandleId key, ValOperandId keyId);
  AttachDecision tryAttachProxyElement(HandleObject obj, ObjOperandId objId,
                                       ValOperandId keyId);

  void trackAttached(const char* name);

 public:
  // NOTE: Argument order is PROPERTY, OBJECT
  HasPropIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc,
                     ICState::Mode mode, CacheKind cacheKind, HandleValue idVal,
                     HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII InstanceOfIRGenerator : public IRGenerator {
  HandleValue lhsVal_;
  HandleObject rhsObj_;

  void trackAttached(const char* name);

 public:
  InstanceOfIRGenerator(JSContext*, HandleScript, jsbytecode*, ICState::Mode,
                        HandleValue, HandleObject);

  AttachDecision tryAttachStub();
};

class MOZ_RAII TypeOfIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachPrimitive(ValOperandId valId);
  AttachDecision tryAttachObject(ValOperandId valId);
  void trackAttached(const char* name);

 public:
  TypeOfIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                    ICState::Mode mode, HandleValue value);

  AttachDecision tryAttachStub();
};

class MOZ_RAII GetIteratorIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachNativeIterator(ObjOperandId objId, HandleObject obj);

 public:
  GetIteratorIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                         ICState::Mode mode, HandleValue value);

  AttachDecision tryAttachStub();

  void trackAttached(const char* name);
};

class MOZ_RAII CallIRGenerator : public IRGenerator {
 private:
  JSOp op_;
  uint32_t argc_;
  HandleValue callee_;
  HandleValue thisval_;
  HandleValue newTarget_;
  HandleValueArray args_;
  PropertyTypeCheckInfo typeCheckInfo_;
  BaselineCacheIRStubKind cacheIRStubKind_;

  bool getTemplateObjectForScripted(HandleFunction calleeFunc,
                                    MutableHandleObject result,
                                    bool* skipAttach);
  bool getTemplateObjectForNative(HandleFunction calleeFunc,
                                  MutableHandleObject result);

  AttachDecision tryAttachArrayPush();
  AttachDecision tryAttachArrayJoin();
  AttachDecision tryAttachIsSuspendedGenerator();
  AttachDecision tryAttachFunCall(HandleFunction calleeFunc);
  AttachDecision tryAttachFunApply(HandleFunction calleeFunc);
  AttachDecision tryAttachCallScripted(HandleFunction calleeFunc);
  AttachDecision tryAttachSpecialCaseCallNative(HandleFunction calleeFunc);
  AttachDecision tryAttachCallNative(HandleFunction calleeFunc);
  AttachDecision tryAttachCallHook(HandleObject calleeObj);

  void trackAttached(const char* name);

 public:
  CallIRGenerator(JSContext* cx, HandleScript script, jsbytecode* pc, JSOp op,
                  ICState::Mode mode, uint32_t argc, HandleValue callee,
                  HandleValue thisval, HandleValue newTarget,
                  HandleValueArray args);

  AttachDecision tryAttachStub();

  AttachDecision tryAttachDeferredStub(HandleValue result);

  BaselineCacheIRStubKind cacheIRStubKind() const { return cacheIRStubKind_; }

  const PropertyTypeCheckInfo* typeCheckInfo() const { return &typeCheckInfo_; }
};

class MOZ_RAII CompareIRGenerator : public IRGenerator {
  JSOp op_;
  HandleValue lhsVal_;
  HandleValue rhsVal_;

  AttachDecision tryAttachString(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachObject(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachSymbol(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachStrictDifferentTypes(ValOperandId lhsId,
                                               ValOperandId rhsId);
  AttachDecision tryAttachInt32(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachNumber(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachBigInt(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachNumberUndefined(ValOperandId lhsId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachPrimitiveUndefined(ValOperandId lhsId,
                                             ValOperandId rhsId);
  AttachDecision tryAttachObjectUndefined(ValOperandId lhsId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachNullUndefined(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachStringNumber(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachPrimitiveSymbol(ValOperandId lhsId,
                                          ValOperandId rhsId);
  AttachDecision tryAttachBoolStringOrNumber(ValOperandId lhsId,
                                             ValOperandId rhsId);
  AttachDecision tryAttachBigIntInt32(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachBigIntNumber(ValOperandId lhsId, ValOperandId rhsId);
  AttachDecision tryAttachBigIntString(ValOperandId lhsId, ValOperandId rhsId);

  void trackAttached(const char* name);

 public:
  CompareIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                     ICState::Mode mode, JSOp op, HandleValue lhsVal,
                     HandleValue rhsVal);

  AttachDecision tryAttachStub();
};

class MOZ_RAII ToBoolIRGenerator : public IRGenerator {
  HandleValue val_;

  AttachDecision tryAttachInt32();
  AttachDecision tryAttachDouble();
  AttachDecision tryAttachString();
  AttachDecision tryAttachSymbol();
  AttachDecision tryAttachNullOrUndefined();
  AttachDecision tryAttachObject();
  AttachDecision tryAttachBigInt();

  void trackAttached(const char* name);

 public:
  ToBoolIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                    ICState::Mode mode, HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII GetIntrinsicIRGenerator : public IRGenerator {
  HandleValue val_;

  void trackAttached(const char* name);

 public:
  GetIntrinsicIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                          ICState::Mode, HandleValue val);

  AttachDecision tryAttachStub();
};

class MOZ_RAII UnaryArithIRGenerator : public IRGenerator {
  JSOp op_;
  HandleValue val_;
  HandleValue res_;

  AttachDecision tryAttachInt32();
  AttachDecision tryAttachNumber();
  AttachDecision tryAttachBigInt();
  AttachDecision tryAttachStringInt32();
  AttachDecision tryAttachStringNumber();

  void trackAttached(const char* name);

 public:
  UnaryArithIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                        ICState::Mode mode, JSOp op, HandleValue val,
                        HandleValue res);

  AttachDecision tryAttachStub();
};

class MOZ_RAII BinaryArithIRGenerator : public IRGenerator {
  JSOp op_;
  HandleValue lhs_;
  HandleValue rhs_;
  HandleValue res_;

  void trackAttached(const char* name);

  AttachDecision tryAttachInt32();
  AttachDecision tryAttachDouble();
  AttachDecision tryAttachBitwise();
  AttachDecision tryAttachStringConcat();
  AttachDecision tryAttachStringObjectConcat();
  AttachDecision tryAttachStringNumberConcat();
  AttachDecision tryAttachStringBooleanConcat();
  AttachDecision tryAttachBigInt();
  AttachDecision tryAttachStringInt32Arith();

 public:
  BinaryArithIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                         ICState::Mode, JSOp op, HandleValue lhs,
                         HandleValue rhs, HandleValue res);

  AttachDecision tryAttachStub();
};

class MOZ_RAII NewObjectIRGenerator : public IRGenerator {
#ifdef JS_CACHEIR_SPEW
  JSOp op_;
#endif
  HandleObject templateObject_;

  void trackAttached(const char* name);

 public:
  NewObjectIRGenerator(JSContext* cx, HandleScript, jsbytecode* pc,
                       ICState::Mode, JSOp op, HandleObject templateObj);

  AttachDecision tryAttachStub();
};

static inline uint32_t SimpleTypeDescrKey(SimpleTypeDescr* descr) {
  if (descr->is<ScalarTypeDescr>()) {
    return uint32_t(descr->as<ScalarTypeDescr>().type()) << 1;
  }
  return (uint32_t(descr->as<ReferenceTypeDescr>().type()) << 1) | 1;
}

inline bool SimpleTypeDescrKeyIsScalar(uint32_t key) { return !(key & 1); }

inline ScalarTypeDescr::Type ScalarTypeFromSimpleTypeDescrKey(uint32_t key) {
  MOZ_ASSERT(SimpleTypeDescrKeyIsScalar(key));
  return ScalarTypeDescr::Type(key >> 1);
}

inline ReferenceType ReferenceTypeFromSimpleTypeDescrKey(uint32_t key) {
  MOZ_ASSERT(!SimpleTypeDescrKeyIsScalar(key));
  return ReferenceType(key >> 1);
}

// Returns whether obj is a WindowProxy wrapping the script's global.
extern bool IsWindowProxyForScriptGlobal(JSScript* script, JSObject* obj);

}  // namespace jit
}  // namespace js

#endif /* jit_CacheIR_h */
