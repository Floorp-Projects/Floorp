/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=0 ft=c:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ObjLiteral_h
#define frontend_ObjLiteral_h

#include "mozilla/EndianUtils.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Span.h"

#include "js/AllocPolicy.h"
#include "js/GCPolicyAPI.h"
#include "js/Value.h"
#include "js/Vector.h"

namespace js {

// Object-literal instruction opcodes. An object literal is constructed by a
// straight-line sequence of these ops, each adding one property to the
// object.
enum class ObjLiteralOpcode : uint8_t {
  INVALID = 0,

  ConstValue = 1,  // numeric types only.
  ConstAtom = 2,
  Null = 3,
  Undefined = 4,
  True = 5,
  False = 6,

  MAX = False,
};

// Flags that are associated with a sequence of object-literal instructions.
// (These become bitflags by wrapping with EnumSet below.)
enum class ObjLiteralFlag : uint8_t {
  // If set, this object is an array.
  Array = 1,

  // If set, the created object will be created with an object group either
  // freshly allocated or determined by property names by calling
  // `ObjectGroup::newPlainObject`.
  SpecificGroup = 2,
  // If set, the created object will be created with newType == SingletonObject
  // rather than TenuredObject.
  Singleton = 3,
  // If set, the created array will be created as a COW array rather than a
  // normal array.
  ArrayCOW = 4,

  // No values are provided; the object is meant as a template object.
  NoValues = 5,
};

using ObjLiteralFlags = mozilla::EnumSet<ObjLiteralFlag>;

inline bool ObjLiteralOpcodeHasValueArg(ObjLiteralOpcode op) {
  return op == ObjLiteralOpcode::ConstValue;
}

inline bool ObjLiteralOpcodeHasAtomArg(ObjLiteralOpcode op) {
  return op == ObjLiteralOpcode::ConstAtom;
}

struct ObjLiteralReaderBase;
// or an integer index.
struct ObjLiteralKey {
 private:
  uint32_t value_;
  bool isArrayIndex_;

 public:
  ObjLiteralKey() : value_(0), isArrayIndex_(true) {}
  ObjLiteralKey(uint32_t value, bool isArrayIndex)
      : value_(value), isArrayIndex_(isArrayIndex) {}
  ObjLiteralKey(const ObjLiteralKey& other) = default;

  static ObjLiteralKey fromPropName(uint32_t atomIndex) {
    return ObjLiteralKey(atomIndex, false);
  }
  static ObjLiteralKey fromArrayIndex(uint32_t index) {
    return ObjLiteralKey(index, true);
  }

  bool isAtomIndex() const { return !isArrayIndex_; }
  bool isArrayIndex() const { return isArrayIndex_; }

  uint32_t getAtomIndex() const {
    MOZ_ASSERT(isAtomIndex());
    return value_;
  }
  uint32_t getArrayIndex() const {
    MOZ_ASSERT(isArrayIndex());
    return value_;
  }

  uint32_t rawIndex() const { return value_; }
};

struct ObjLiteralWriterBase {
 protected:
  friend struct ObjLiteralReaderBase;  // for access to mask and shift.
  static const uint32_t ATOM_INDEX_MASK = 0x007fffff;
  // If set, the atom index field is an array index, not an atom index.
  static const uint32_t INDEXED_PROP = 0x00800000;
  static const int OP_SHIFT = 24;

 protected:
  Vector<uint8_t, 64> code_;

 public:
  explicit ObjLiteralWriterBase(JSContext* cx) : code_(cx) {}

  uint32_t curOffset() const { return code_.length(); }

  MOZ_MUST_USE bool prepareBytes(size_t len, uint8_t** p) {
    size_t offset = code_.length();
    if (!code_.growByUninitialized(len)) {
      return false;
    }
    *p = &code_[offset];
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool pushRawData(T data) {
    uint8_t* p = nullptr;
    if (!prepareBytes(sizeof(T), &p)) {
      return false;
    }
    mozilla::NativeEndian::copyAndSwapToLittleEndian(reinterpret_cast<void*>(p),
                                                     &data, 1);
    return true;
  }

  MOZ_MUST_USE bool pushOpAndName(ObjLiteralOpcode op, ObjLiteralKey key) {
    uint32_t data = (key.rawIndex() & ATOM_INDEX_MASK) |
                    (key.isArrayIndex() ? INDEXED_PROP : 0) |
                    (static_cast<uint8_t>(op) << OP_SHIFT);
    return pushRawData(data);
  }

  MOZ_MUST_USE bool pushValueArg(const JS::Value& value) {
    MOZ_ASSERT(value.isNumber() || value.isNullOrUndefined() ||
               value.isBoolean());
    uint64_t data = value.asRawBits();
    return pushRawData(data);
  }

  MOZ_MUST_USE bool pushAtomArg(uint32_t atomIndex) {
    return pushRawData(atomIndex);
  }
};

// An object-literal instruction writer. This class, held by the bytecode
// emitter, keeps a sequence of object-literal instructions emitted as object
// literal expressions are parsed. It allows the user to 'begin' and 'end'
// straight-line sequences, returning the offsets for this range of instructions
// within the writer.
struct ObjLiteralWriter : private ObjLiteralWriterBase {
 public:
  explicit ObjLiteralWriter(JSContext* cx)
      : ObjLiteralWriterBase(cx), flags_() {}

  void clear() { code_.clear(); }

  mozilla::Span<const uint8_t> getCode() const { return code_; }
  ObjLiteralFlags getFlags() const { return flags_; }

  void beginObject(ObjLiteralFlags flags) { flags_ = flags; }
  void setPropName(uint32_t propName) {
    MOZ_ASSERT(propName <= ATOM_INDEX_MASK);
    nextKey_ = ObjLiteralKey::fromPropName(propName);
  }
  void setPropIndex(uint32_t propIndex) {
    MOZ_ASSERT(propIndex <= ATOM_INDEX_MASK);
    nextKey_ = ObjLiteralKey::fromArrayIndex(propIndex);
  }

  MOZ_MUST_USE bool propWithConstNumericValue(const JS::Value& value) {
    MOZ_ASSERT(value.isNumber());
    return pushOpAndName(ObjLiteralOpcode::ConstValue, nextKey_) &&
           pushValueArg(value);
  }
  MOZ_MUST_USE bool propWithAtomValue(uint32_t value) {
    return pushOpAndName(ObjLiteralOpcode::ConstAtom, nextKey_) &&
           pushAtomArg(value);
  }
  MOZ_MUST_USE bool propWithNullValue() {
    return pushOpAndName(ObjLiteralOpcode::Null, nextKey_);
  }
  MOZ_MUST_USE bool propWithUndefinedValue() {
    return pushOpAndName(ObjLiteralOpcode::Undefined, nextKey_);
  }
  MOZ_MUST_USE bool propWithTrueValue() {
    return pushOpAndName(ObjLiteralOpcode::True, nextKey_);
  }
  MOZ_MUST_USE bool propWithFalseValue() {
    return pushOpAndName(ObjLiteralOpcode::False, nextKey_);
  }

  static bool arrayIndexInRange(int32_t i) {
    return i >= 0 && static_cast<uint32_t>(i) <= ATOM_INDEX_MASK;
  }

 private:
  ObjLiteralFlags flags_;
  ObjLiteralKey nextKey_;
};

struct ObjLiteralReaderBase {
 private:
  mozilla::Span<const uint8_t> data_;
  size_t cursor_;

  MOZ_MUST_USE bool readBytes(size_t size, const uint8_t** p) {
    if (cursor_ + size > data_.Length()) {
      return false;
    }
    *p = data_.From(cursor_).data();
    cursor_ += size;
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool readRawData(T* data) {
    const uint8_t* p = nullptr;
    if (!readBytes(sizeof(T), &p)) {
      return false;
    }
    mozilla::NativeEndian::copyAndSwapFromLittleEndian(
        data, reinterpret_cast<const void*>(p), 1);
    return true;
  }

 public:
  explicit ObjLiteralReaderBase(mozilla::Span<const uint8_t> data)
      : data_(data), cursor_(0) {}

  MOZ_MUST_USE bool readOpAndKey(ObjLiteralOpcode* op, ObjLiteralKey* key) {
    uint32_t data;
    if (!readRawData(&data)) {
      return false;
    }
    uint8_t opbyte =
        static_cast<uint8_t>(data >> ObjLiteralWriterBase::OP_SHIFT);
    if (MOZ_UNLIKELY(opbyte > static_cast<uint8_t>(ObjLiteralOpcode::MAX))) {
      return false;
    }
    *op = static_cast<ObjLiteralOpcode>(opbyte);
    bool isArray = data & ObjLiteralWriterBase::INDEXED_PROP;
    uint32_t rawIndex = data & ObjLiteralWriterBase::ATOM_INDEX_MASK;
    *key = ObjLiteralKey(rawIndex, isArray);
    return true;
  }

  MOZ_MUST_USE bool readValueArg(JS::Value* value) {
    uint64_t data;
    if (!readRawData(&data)) {
      return false;
    }
    *value = JS::Value::fromRawBits(data);
    return true;
  }

  MOZ_MUST_USE bool readAtomArg(uint32_t* atomIndex) {
    return readRawData(atomIndex);
  }
};

// A single object-literal instruction, creating one property on an object.
struct ObjLiteralInsn {
 private:
  ObjLiteralOpcode op_;
  ObjLiteralKey key_;
  union Arg {
    explicit Arg(uint64_t raw_) : raw(raw_) {}

    JS::Value constValue;
    uint32_t atomIndex;
    uint64_t raw;
  } arg_;

 public:
  ObjLiteralInsn() : op_(ObjLiteralOpcode::INVALID), arg_(0) {}
  ObjLiteralInsn(ObjLiteralOpcode op, ObjLiteralKey key)
      : op_(op), key_(key), arg_(0) {
    MOZ_ASSERT(!hasConstValue());
    MOZ_ASSERT(!hasAtomIndex());
  }
  ObjLiteralInsn(ObjLiteralOpcode op, ObjLiteralKey key, const JS::Value& value)
      : op_(op), key_(key), arg_(0) {
    MOZ_ASSERT(hasConstValue());
    MOZ_ASSERT(!hasAtomIndex());
    arg_.constValue = value;
  }
  ObjLiteralInsn(ObjLiteralOpcode op, ObjLiteralKey key, uint32_t atomIndex)
      : op_(op), key_(key), arg_(0) {
    MOZ_ASSERT(!hasConstValue());
    MOZ_ASSERT(hasAtomIndex());
    arg_.atomIndex = atomIndex;
  }
  ObjLiteralInsn(const ObjLiteralInsn& other) : ObjLiteralInsn() {
    *this = other;
  }
  ObjLiteralInsn& operator=(const ObjLiteralInsn& other) {
    op_ = other.op_;
    key_ = other.key_;
    arg_.raw = other.arg_.raw;
    return *this;
  }

  bool isValid() const {
    return op_ > ObjLiteralOpcode::INVALID && op_ <= ObjLiteralOpcode::MAX;
  }

  ObjLiteralOpcode getOp() const {
    MOZ_ASSERT(isValid());
    return op_;
  }
  const ObjLiteralKey& getKey() const {
    MOZ_ASSERT(isValid());
    return key_;
  }

  bool hasConstValue() const {
    MOZ_ASSERT(isValid());
    return ObjLiteralOpcodeHasValueArg(op_);
  }
  bool hasAtomIndex() const {
    MOZ_ASSERT(isValid());
    return ObjLiteralOpcodeHasAtomArg(op_);
  }

  JS::Value getConstValue() const {
    MOZ_ASSERT(isValid());
    MOZ_ASSERT(hasConstValue());
    return arg_.constValue;
  }
  uint32_t getAtomIndex() const {
    MOZ_ASSERT(isValid());
    MOZ_ASSERT(hasAtomIndex());
    return arg_.atomIndex;
  };
};

// A reader that parses a sequence of object-literal instructions out of the
// encoded form.
struct ObjLiteralReader : private ObjLiteralReaderBase {
 public:
  explicit ObjLiteralReader(mozilla::Span<const uint8_t> data)
      : ObjLiteralReaderBase(data) {}

  MOZ_MUST_USE bool readInsn(ObjLiteralInsn* insn) {
    ObjLiteralOpcode op;
    ObjLiteralKey key;
    if (!readOpAndKey(&op, &key)) {
      return false;
    }
    if (ObjLiteralOpcodeHasValueArg(op)) {
      JS::Value value;
      if (!readValueArg(&value)) {
        return false;
      }
      *insn = ObjLiteralInsn(op, key, value);
      return true;
    }
    if (ObjLiteralOpcodeHasAtomArg(op)) {
      uint32_t atomIndex;
      if (!readAtomArg(&atomIndex)) {
        return false;
      }
      *insn = ObjLiteralInsn(op, key, atomIndex);
      return true;
    }
    *insn = ObjLiteralInsn(op, key);
    return true;
  }
};

typedef Vector<JSAtom*, 4> ObjLiteralAtomVector;

JSObject* InterpretObjLiteral(JSContext* cx, ObjLiteralAtomVector& atoms,
                              mozilla::Span<const uint8_t> insns,
                              ObjLiteralFlags flags);

inline JSObject* InterpretObjLiteral(JSContext* cx, ObjLiteralAtomVector& atoms,
                                     const ObjLiteralWriter& writer) {
  return InterpretObjLiteral(cx, atoms, writer.getCode(), writer.getFlags());
}

class ObjLiteralCreationData {
 private:
  ObjLiteralWriter writer_;
  ObjLiteralAtomVector atoms_;

 public:
  explicit ObjLiteralCreationData(JSContext* cx) : writer_(cx), atoms_(cx) {}

  ObjLiteralWriter& writer() { return writer_; }

  bool addAtom(JSAtom* atom, uint32_t* index) {
    *index = atoms_.length();
    return atoms_.append(atom);
  }

  JSObject* create(JSContext* cx);
};

}  // namespace js

namespace JS {
// Ignore GC tracing for the ObjLiteralCreationData. It contains JSAtom
// pointers, but these are already held and rooted by the parser. (We must
// specify GC policy for the creation data because it is placed in the
// GC-things vector.)
template <>
struct GCPolicy<js::ObjLiteralCreationData>
    : JS::IgnoreGCPolicy<js::ObjLiteralCreationData> {};
}  // namespace JS

#endif  // frontend_ObjLiteral_h
