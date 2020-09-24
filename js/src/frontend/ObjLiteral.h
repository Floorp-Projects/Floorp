/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "frontend/ParserAtom.h"
#include "js/AllocPolicy.h"
#include "js/GCPolicyAPI.h"
#include "js/Value.h"
#include "js/Vector.h"

/*
 * [SMDOC] ObjLiteral (Object Literal) Handling
 * ============================================
 *
 * The `ObjLiteral*` family of classes defines an infastructure to handle
 * object literals as they are encountered at parse time and translate them
 * into objects that are attached to the bytecode.
 *
 * The object-literal "instructions", whose opcodes are defined in
 * `ObjLiteralOpcode` below, each specify one key (atom property name, or
 * numeric index) and one value. An `ObjLiteralWriter` buffers a linear
 * sequence of such instructions, along with a side-table of atom references.
 * The writer stores a compact binary format that is then interpreted by the
 * `ObjLiteralReader` to construct an object according to the instructions.
 *
 * This may seem like an odd dance: create an intermediate data structure that
 * specifies key/value pairs, then later build the object. Why not just do so
 * directly, as we parse? In fact, we used to do this. However, for several
 * good reasons, we want to avoid allocating or touching GC objects at all
 * *during* the parse. We thus use a sequence of ObjLiteral instructions as an
 * intermediate data structure to carry object literal contents from parse to
 * the time at which we *can* allocate objects.
 *
 * (The original intent was to allow for ObjLiteral instructions to actually be
 * invoked by a new JS opcode, JSOp::ObjLiteral, thus replacing the more
 * general opcode sequences sometimes generated to fill in objects and removing
 * the need to attach actual objects to JSOp::Object or JSOp::NewObject.
 * However, this was far too invasive and led to performance regressions, so
 * currently ObjLiteral only carries literals as far as the end of the parse
 * pipeline, when all GC things are allocated.)
 *
 * ObjLiteral data structures are used to represent object literals whenever
 * they are "compatible". See
 * BytecodeEmitter::isPropertyListObjLiteralCompatible for the precise
 * conditions; in brief, we can represent object literals with "primitive"
 * (numeric, boolean, string, null/undefined) values, and "normal"
 * (non-computed) object names. We can also represent arrays with the same
 * value restrictions. We cannot represent nested objects. We use ObjLiteral in
 * two different ways:
 *
 * - To build a template object, when we can support the properties but not the
 *   keys.
 * - To build the actual result object, when we support the properties and the
 *   keys and this is a JSOp::Object case (see below).
 *
 * Design and Performance Considerations
 * -------------------------------------
 *
 * As a brief overview, there are a number of opcodes that allocate objects:
 *
 * - JSOp::NewInit allocates a new empty `{}` object.
 *
 * - JSOp::NewObject, with an object as an argument (held by the script data
 *   side-tables), allocates a new object with `undefined` property values but
 *   with a defined set of properties. The given object is used as a
 *   *template*.
 *
 * - JSOp::NewObjectWithGroup (added as part of this ObjLiteral work), same as
 *   above but uses the ObjectGroup of the template object for the new object,
 *   rather than trying to apply a set of heuristics to choose a group.
 *
 * - JSOp::Object, with an object as argument, instructs the runtime to
 *   literally return the object argument as the result. This is thus only an
 *   "allocation" in the sense that the object was originally allocated when
 *   the script data / bytecode was created. It is only used when we know for
 *   sure that the script, and this program point within the script, will run
 *   *once*. (See the `treatAsRunOnce` flag on JSScript.)
 *
 * Before we go further, we also define "singleton context" and "singleton
 * group". An operation occurs in a "singleton context", according to the
 * parser, if it will only ever execute once. In particular, this happens when
 * (i) the script is a "run-once" script, which is usually the case for e.g.
 * top-level scripts of web-pages (they run on page load, but no function or
 * handle wraps or refers to the script so it can't be invoked again),
 * and (ii) the operation itself is not within a loop or function in that
 * run-once script. "Singleton group", on the other hand, refers to an
 * ObjectGroup (used by Type Inference) that represents only one object, and
 * has a special flag set to mark it as such. Usually we want to give singleton
 * groups to object allocations that happen in a singleton context (because
 * there will only ever be one of the object), hence the connection between
 * these terms.
 *
 * When we encounter an object literal, we decide which opcode to use, and we
 * construct the ObjLiteral and the bytecode using its result appropriately:
 *
 * - If in a singleton context, and if we support the values, we use
 *   JSOp::Object and we build the ObjLiteral instructions with values.
 * - Otherwise, if we support the keys but not the values, or if we are not
 *   in a singleton context, we use JSOp::NewObject or JSOp::NewObjectWithGroup,
 *   depending on the "inner singleton" status (see below). In this case, the
 *   initial opcode only creates an object with empty values, so
 *   BytecodeEmitter then generates bytecode to set the values
 *   appropriately.
 * - Otherwise, we generate JSOp::NewInit and bytecode to add properties one at
 *   a time. This will always work, but is the slowest and least
 *   memory-efficient option.
 *
 * We need to take special care to ensure that the ObjectGroup of the resulting
 * object is chosen "correctly". Failure to do so can result in all sorts
 * of performance and/or memory regressions. In brief, we want to use a
 * singleton group whenever an object is allocated in a singleton context.
 * However, there is a special "inner singleton" context that deserves special
 * mention. When a program has a nested tree of objects, the old
 * (pre-ObjLiteral) world would perform a group lookup by shape (list of
 * property IDs) for all non-root objects, so in the following snippet, the
 * inner objects would share a group:
 *
 *     var list = [{a: 0}, {a: 1}];
 *     var obj = { first: {a: 0}, second: {a: 1} };
 *
 * In the generated bytecode, the inner objects are created first, then placed
 * in the relevant properties of the outer objects/arrays using
 * INITPROP/INITELEM. Thus to a naïve analysis, it appears that the inner
 * objects are singletons. But heuristically it is better if they are not. So
 * we pass down an `isInner` boolean while recursively traversing the
 * parse-node tree and generating bytecode. If we encounter an object literal
 * that is in singleton (run-once) context but also `isInner`, then we set
 * special flags to ensure its shape is looked up based on properties instead.
 */

namespace js {

class JSONPrinter;

namespace frontend {
struct CompilationInfo;
class StencilXDR;
}  // namespace frontend

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

  // This object is inside a top-level singleton, and so prior to ObjLiteral,
  // would have been allocated at parse time, but is now allocated in bytecode.
  // We do special things to get the right group on the template object; this
  // flag indicates that if JSOp::NewObject copies the object, it should retain
  // its group.
  IsInnerSingleton = 6,
};

using ObjLiteralFlags = mozilla::EnumSet<ObjLiteralFlag>;

inline bool ObjLiteralOpcodeHasValueArg(ObjLiteralOpcode op) {
  return op == ObjLiteralOpcode::ConstValue;
}

inline bool ObjLiteralOpcodeHasAtomArg(ObjLiteralOpcode op) {
  return op == ObjLiteralOpcode::ConstAtom;
}

struct ObjLiteralReaderBase;

// Property name (as an atom index) or an integer index.  Only used for
// object-type literals; array literals do not require the index (the sequence
// is always dense, with no holes, so the index is implicit). For the latter
// case, we have a `None` placeholder.
struct ObjLiteralKey {
 private:
  uint32_t value_;

  enum ObjLiteralKeyType {
    None,
    AtomIndex,
    ArrayIndex,
  };

  ObjLiteralKeyType type_;

  ObjLiteralKey(uint32_t value, ObjLiteralKeyType ty)
      : value_(value), type_(ty) {}

 public:
  ObjLiteralKey() : ObjLiteralKey(0, None) {}
  ObjLiteralKey(uint32_t value, bool isArrayIndex)
      : ObjLiteralKey(value, isArrayIndex ? ArrayIndex : AtomIndex) {}
  ObjLiteralKey(const ObjLiteralKey& other) = default;

  static ObjLiteralKey fromPropName(uint32_t atomIndex) {
    return ObjLiteralKey(atomIndex, false);
  }
  static ObjLiteralKey fromArrayIndex(uint32_t index) {
    return ObjLiteralKey(index, true);
  }
  static ObjLiteralKey none() { return ObjLiteralKey(); }

  bool isNone() const { return type_ == None; }
  bool isAtomIndex() const { return type_ == AtomIndex; }
  bool isArrayIndex() const { return type_ == ArrayIndex; }

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

 public:
  using CodeVector = Vector<uint8_t, 64, js::SystemAllocPolicy>;

 protected:
  CodeVector code_;

 public:
  ObjLiteralWriterBase() = default;

  uint32_t curOffset() const { return code_.length(); }

  MOZ_MUST_USE bool prepareBytes(JSContext* cx, size_t len, uint8_t** p) {
    size_t offset = code_.length();
    if (!code_.growByUninitialized(len)) {
      js::ReportOutOfMemory(cx);
      return false;
    }
    *p = &code_[offset];
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool pushRawData(JSContext* cx, T data) {
    uint8_t* p = nullptr;
    if (!prepareBytes(cx, sizeof(T), &p)) {
      return false;
    }
    mozilla::NativeEndian::copyAndSwapToLittleEndian(reinterpret_cast<void*>(p),
                                                     &data, 1);
    return true;
  }

  MOZ_MUST_USE bool pushOpAndName(JSContext* cx, ObjLiteralOpcode op,
                                  ObjLiteralKey key) {
    uint32_t data = (key.rawIndex() & ATOM_INDEX_MASK) |
                    (key.isArrayIndex() ? INDEXED_PROP : 0) |
                    (static_cast<uint8_t>(op) << OP_SHIFT);
    return pushRawData(cx, data);
  }

  MOZ_MUST_USE bool pushValueArg(JSContext* cx, const JS::Value& value) {
    MOZ_ASSERT(value.isNumber() || value.isNullOrUndefined() ||
               value.isBoolean());
    uint64_t data = value.asRawBits();
    return pushRawData(cx, data);
  }

  MOZ_MUST_USE bool pushAtomArg(JSContext* cx, uint32_t atomIndex) {
    return pushRawData(cx, atomIndex);
  }
};

// An object-literal instruction writer. This class, held by the bytecode
// emitter, keeps a sequence of object-literal instructions emitted as object
// literal expressions are parsed. It allows the user to 'begin' and 'end'
// straight-line sequences, returning the offsets for this range of instructions
// within the writer.
struct ObjLiteralWriter : private ObjLiteralWriterBase {
 public:
  ObjLiteralWriter() = default;

  void clear() { code_.clear(); }

  // For XDR decoding.
  using CodeVector = typename ObjLiteralWriterBase::CodeVector;
  void initializeForXDR(CodeVector&& code, uint8_t flags) {
    code_ = std::move(code);
    flags_.deserialize(flags);
  }

  mozilla::Span<const uint8_t> getCode() const { return code_; }
  ObjLiteralFlags getFlags() const { return flags_; }

  void beginObject(ObjLiteralFlags flags) { flags_ = flags; }
  void setPropName(uint32_t propName) {
    // Only valid in object-mode.
    MOZ_ASSERT(!flags_.contains(ObjLiteralFlag::Array));
    MOZ_ASSERT(propName <= ATOM_INDEX_MASK);
    nextKey_ = ObjLiteralKey::fromPropName(propName);
  }
  void setPropIndex(uint32_t propIndex) {
    // Only valid in object-mode.
    MOZ_ASSERT(!flags_.contains(ObjLiteralFlag::Array));
    MOZ_ASSERT(propIndex <= ATOM_INDEX_MASK);
    nextKey_ = ObjLiteralKey::fromArrayIndex(propIndex);
  }
  void beginDenseArrayElements() {
    // Only valid in array-mode.
    MOZ_ASSERT(flags_.contains(ObjLiteralFlag::Array));
    // Dense array element sequences do not use the keys; the indices are
    // implicit.
    nextKey_ = ObjLiteralKey::none();
  }

  MOZ_MUST_USE bool propWithConstNumericValue(JSContext* cx,
                                              const JS::Value& value) {
    MOZ_ASSERT(value.isNumber());
    return pushOpAndName(cx, ObjLiteralOpcode::ConstValue, nextKey_) &&
           pushValueArg(cx, value);
  }
  MOZ_MUST_USE bool propWithAtomValue(JSContext* cx, uint32_t value) {
    return pushOpAndName(cx, ObjLiteralOpcode::ConstAtom, nextKey_) &&
           pushAtomArg(cx, value);
  }
  MOZ_MUST_USE bool propWithNullValue(JSContext* cx) {
    return pushOpAndName(cx, ObjLiteralOpcode::Null, nextKey_);
  }
  MOZ_MUST_USE bool propWithUndefinedValue(JSContext* cx) {
    return pushOpAndName(cx, ObjLiteralOpcode::Undefined, nextKey_);
  }
  MOZ_MUST_USE bool propWithTrueValue(JSContext* cx) {
    return pushOpAndName(cx, ObjLiteralOpcode::True, nextKey_);
  }
  MOZ_MUST_USE bool propWithFalseValue(JSContext* cx) {
    return pushOpAndName(cx, ObjLiteralOpcode::False, nextKey_);
  }

  static bool arrayIndexInRange(int32_t i) {
    return i >= 0 && static_cast<uint32_t>(i) <= ATOM_INDEX_MASK;
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json);
  void dumpFields(JSONPrinter& json);
#endif

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

typedef Vector<const frontend::ParserAtom*, 4, js::SystemAllocPolicy>
    ObjLiteralAtomVector;

JSObject* InterpretObjLiteral(JSContext* cx,
                              frontend::CompilationInfo& compilationInfo,
                              const ObjLiteralAtomVector& atoms,
                              const mozilla::Span<const uint8_t> insns,
                              ObjLiteralFlags flags);

inline JSObject* InterpretObjLiteral(JSContext* cx,
                                     frontend::CompilationInfo& compilationInfo,
                                     const ObjLiteralAtomVector& atoms,
                                     const ObjLiteralWriter& writer) {
  return InterpretObjLiteral(cx, compilationInfo, atoms, writer.getCode(),
                             writer.getFlags());
}

class ObjLiteralStencil {
  friend class frontend::StencilXDR;

  ObjLiteralWriter writer_;
  ObjLiteralAtomVector atoms_;

 public:
  ObjLiteralStencil() = default;

  ObjLiteralWriter& writer() { return writer_; }

  bool addAtom(JSContext* cx, const frontend::ParserAtom* atom,
               uint32_t* index) {
    *index = atoms_.length();
    if (!atoms_.append(atom)) {
      js::ReportOutOfMemory(cx);
      return false;
    }
    return true;
  }

  JSObject* create(JSContext* cx, frontend::CompilationInfo& info) const;

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json);
  void dumpFields(JSONPrinter& json);
#endif
};

}  // namespace js
#endif  // frontend_ObjLiteral_h
