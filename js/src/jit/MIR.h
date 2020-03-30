/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Everything needed to build actual MIR instructions: the actual opcodes and
 * instructions, the instruction interface, and use chains.
 */

#ifndef jit_MIR_h
#define jit_MIR_h

#include "mozilla/Alignment.h"
#include "mozilla/Array.h"
#include "mozilla/Attributes.h"
#include "mozilla/MacroForEach.h"

#include <algorithm>

#include "jit/AtomicOp.h"
#include "jit/BaselineIC.h"
#include "jit/FixedList.h"
#include "jit/InlineList.h"
#include "jit/JitAllocPolicy.h"
#include "jit/MacroAssembler.h"
#include "jit/MOpcodes.h"
#include "jit/TIOracle.h"
#include "jit/TypePolicy.h"
#include "js/HeapAPI.h"
#include "vm/ArrayObject.h"
#include "vm/EnvironmentObject.h"
#include "vm/RegExpObject.h"
#include "vm/SharedMem.h"
#include "vm/TypedArrayObject.h"

namespace js {

namespace wasm {
class FuncExport;
}

class StringObject;

namespace jit {

// Forward declarations of MIR types.
#define FORWARD_DECLARE(op) class M##op;
MIR_OPCODE_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

// MDefinition visitor which ignores non-overloaded visit functions.
class MDefinitionVisitorDefaultNoop {
 public:
#define VISIT_INS(op) \
  void visit##op(M##op*) {}
  MIR_OPCODE_LIST(VISIT_INS)
#undef VISIT_INS
};

class BaselineInspector;
class Range;

template <typename T>
struct ResultWithOOM {
  T value;
  bool oom;

  static ResultWithOOM<T> ok(T val) { return {val, false}; }
  static ResultWithOOM<T> fail() { return {T(), true}; }
};

static inline MIRType MIRTypeFromValue(const js::Value& vp) {
  if (vp.isDouble()) {
    return MIRType::Double;
  }
  if (vp.isMagic()) {
    switch (vp.whyMagic()) {
      case JS_OPTIMIZED_ARGUMENTS:
        return MIRType::MagicOptimizedArguments;
      case JS_OPTIMIZED_OUT:
        return MIRType::MagicOptimizedOut;
      case JS_ELEMENTS_HOLE:
        return MIRType::MagicHole;
      case JS_IS_CONSTRUCTING:
        return MIRType::MagicIsConstructing;
      case JS_UNINITIALIZED_LEXICAL:
        return MIRType::MagicUninitializedLexical;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected magic constant");
    }
  }
  return MIRTypeFromValueType(vp.extractNonDoubleType());
}

#define MIR_FLAG_LIST(_)                                                       \
  _(InWorklist)                                                                \
  _(EmittedAtUses)                                                             \
  _(Commutative)                                                               \
  _(Movable) /* Allow passes like LICM to move this instruction */             \
  _(Lowered) /* (Debug only) has a virtual register */                         \
  _(Guard)   /* Not removable if uses == 0 */                                  \
                                                                               \
  /* Flag an instruction to be considered as a Guard if the instructions       \
   * bails out on some inputs.                                                 \
   *                                                                           \
   * Some optimizations can replace an instruction, and leave its operands     \
   * unused. When the type information of the operand got used as a            \
   * predicate of the transformation, then we have to flag the operands as     \
   * GuardRangeBailouts.                                                       \
   *                                                                           \
   * This flag prevents further optimization of instructions, which            \
   * might remove the run-time checks (bailout conditions) used as a           \
   * predicate of the previous transformation.                                 \
   */                                                                          \
  _(GuardRangeBailouts)                                                        \
                                                                               \
  /* Some instructions have uses that aren't directly represented in the       \
   * graph, and need to be handled specially. As an example, this is used to   \
   * keep the flagged instruction in resume points, not substituting with an   \
   * UndefinedValue. This can be used by call inlining when a function         \
   * argument is not used by the inlined instructions.                         \
   */                                                                          \
  _(ImplicitlyUsed)                                                            \
                                                                               \
  /* The instruction has been marked dead for lazy removal from resume         \
   * points.                                                                   \
   */                                                                          \
  _(Unused)                                                                    \
                                                                               \
  /* When a branch is removed, the uses of multiple instructions are removed.  \
   * The removal of branches is based on hypotheses.  These hypotheses might   \
   * fail, in which case we need to bailout from the current code.             \
   *                                                                           \
   * When we implement a destructive optimization, we need to consider the     \
   * failing cases, and consider the fact that we might resume the execution   \
   * into a branch which was removed from the compiler.  As such, a            \
   * destructive optimization need to take into acount removed branches.       \
   *                                                                           \
   * In order to let destructive optimizations know about removed branches, we \
   * have to annotate instructions with the UseRemoved flag.  This flag        \
   * annotates instruction which were used in removed branches.                \
   */                                                                          \
  _(UseRemoved)                                                                \
                                                                               \
  /* Marks if the current instruction should go to the bailout paths instead   \
   * of producing code as part of the control flow.  This flag can only be set \
   * on instructions which are only used by ResumePoint or by other flagged    \
   * instructions.                                                             \
   */                                                                          \
  _(RecoveredOnBailout)                                                        \
                                                                               \
  /* Some instructions might represent an object, but the memory of these      \
   * objects might be incomplete if we have not recovered all the stores which \
   * were supposed to happen before. This flag is used to annotate             \
   * instructions which might return a pointer to a memory area which is not   \
   * yet fully initialized. This flag is used to ensure that stores are        \
   * executed before returning the value.                                      \
   */                                                                          \
  _(IncompleteObject)                                                          \
                                                                               \
  /* For WebAssembly, there are functions with multiple results.  Instead of   \
   * having the results defined by one call instruction, they are instead      \
   * captured in subsequent result capture instructions, because modelling     \
   * multi-value results in Ion is too complicated.  However since they        \
   * capture ambient live registers, it would be an error to move an unrelated \
   * instruction between the call and the result capture.  This flag is used   \
   * to prevent code motion from moving instructions in invalid ways.          \
   */                                                                          \
  _(CallResultCapture)                                                         \
                                                                               \
  /* The current instruction got discarded from the MIR Graph. This is useful  \
   * when we want to iterate over resume points and instructions, while        \
   * handling instructions which are discarded without reporting to the        \
   * iterator.                                                                 \
   */                                                                          \
  _(Discarded)

class MDefinition;
class MInstruction;
class MBasicBlock;
class MNode;
class MUse;
class MPhi;
class MIRGraph;
class MResumePoint;
class MControlInstruction;

// Represents a use of a node.
class MUse : public TempObject, public InlineListNode<MUse> {
  // Grant access to setProducerUnchecked.
  friend class MDefinition;
  friend class MPhi;

  MDefinition* producer_;  // MDefinition that is being used.
  MNode* consumer_;        // The node that is using this operand.

  // Low-level unchecked edit method for replaceAllUsesWith and
  // MPhi::removeOperand. This doesn't update use lists!
  // replaceAllUsesWith and MPhi::removeOperand do that manually.
  void setProducerUnchecked(MDefinition* producer) {
    MOZ_ASSERT(consumer_);
    MOZ_ASSERT(producer_);
    MOZ_ASSERT(producer);
    producer_ = producer;
  }

 public:
  // Default constructor for use in vectors.
  MUse() : producer_(nullptr), consumer_(nullptr) {}

  // Move constructor for use in vectors. When an MUse is moved, it stays
  // in its containing use list.
  MUse(MUse&& other)
      : InlineListNode<MUse>(std::move(other)),
        producer_(other.producer_),
        consumer_(other.consumer_) {}

  // Construct an MUse initialized with |producer| and |consumer|.
  MUse(MDefinition* producer, MNode* consumer) {
    initUnchecked(producer, consumer);
  }

  // Set this use, which was previously clear.
  inline void init(MDefinition* producer, MNode* consumer);
  // Like init, but works even when the use contains uninitialized data.
  inline void initUnchecked(MDefinition* producer, MNode* consumer);
  // Like initUnchecked, but set the producer to nullptr.
  inline void initUncheckedWithoutProducer(MNode* consumer);
  // Set this use, which was not previously clear.
  inline void replaceProducer(MDefinition* producer);
  // Clear this use.
  inline void releaseProducer();

  MDefinition* producer() const {
    MOZ_ASSERT(producer_ != nullptr);
    return producer_;
  }
  bool hasProducer() const { return producer_ != nullptr; }
  MNode* consumer() const {
    MOZ_ASSERT(consumer_ != nullptr);
    return consumer_;
  }

#ifdef DEBUG
  // Return the operand index of this MUse in its consumer. This is DEBUG-only
  // as normal code should instead call indexOf on the cast consumer directly,
  // to allow it to be devirtualized and inlined.
  size_t index() const;
#endif
};

using MUseIterator = InlineList<MUse>::iterator;

// A node is an entry in the MIR graph. It has two kinds:
//   MInstruction: an instruction which appears in the IR stream.
//   MResumePoint: a list of instructions that correspond to the state of the
//                 interpreter/Baseline stack.
//
// Nodes can hold references to MDefinitions. Each MDefinition has a list of
// nodes holding such a reference (its use chain).
class MNode : public TempObject {
 protected:
  enum class Kind { Definition = 0, ResumePoint };

 private:
  static const uintptr_t KindMask = 0x1;
  uintptr_t blockAndKind_;

  Kind kind() const { return Kind(blockAndKind_ & KindMask); }

 protected:
  explicit MNode(const MNode& other) : blockAndKind_(other.blockAndKind_) {}

  MNode(MBasicBlock* block, Kind kind) { setBlockAndKind(block, kind); }

  void setBlockAndKind(MBasicBlock* block, Kind kind) {
    blockAndKind_ = uintptr_t(block) | uintptr_t(kind);
    MOZ_ASSERT(this->block() == block);
  }

  MBasicBlock* definitionBlock() const {
    MOZ_ASSERT(isDefinition());
    static_assert(unsigned(Kind::Definition) == 0,
                  "Code below relies on low bit being 0");
    return reinterpret_cast<MBasicBlock*>(blockAndKind_);
  }
  MBasicBlock* resumePointBlock() const {
    MOZ_ASSERT(isResumePoint());
    static_assert(unsigned(Kind::ResumePoint) == 1,
                  "Code below relies on low bit being 1");
    // Use a subtraction: if the caller does block()->foo, the compiler
    // will be able to fold it with the load.
    return reinterpret_cast<MBasicBlock*>(blockAndKind_ - 1);
  }

 public:
  // Returns the definition at a given operand.
  virtual MDefinition* getOperand(size_t index) const = 0;
  virtual size_t numOperands() const = 0;
  virtual size_t indexOf(const MUse* u) const = 0;

  bool isDefinition() const { return kind() == Kind::Definition; }
  bool isResumePoint() const { return kind() == Kind::ResumePoint; }
  MBasicBlock* block() const {
    return reinterpret_cast<MBasicBlock*>(blockAndKind_ & ~KindMask);
  }
  MBasicBlock* caller() const;

  // Sets an already set operand, updating use information. If you're looking
  // for setOperand, this is probably what you want.
  virtual void replaceOperand(size_t index, MDefinition* operand) = 0;

  // Resets the operand to an uninitialized state, breaking the link
  // with the previous operand's producer.
  void releaseOperand(size_t index) { getUseFor(index)->releaseProducer(); }
  bool hasOperand(size_t index) const {
    return getUseFor(index)->hasProducer();
  }

  inline MDefinition* toDefinition();
  inline MResumePoint* toResumePoint();

  virtual MOZ_MUST_USE bool writeRecoverData(CompactBufferWriter& writer) const;

#ifdef JS_JITSPEW
  virtual void dump(GenericPrinter& out) const = 0;
  virtual void dump() const = 0;
#endif

 protected:
  // Need visibility on getUseFor to avoid O(n^2) complexity.
  friend void AssertBasicGraphCoherency(MIRGraph& graph, bool force);

  // Gets the MUse corresponding to given operand.
  virtual MUse* getUseFor(size_t index) = 0;
  virtual const MUse* getUseFor(size_t index) const = 0;
};

class AliasSet {
 private:
  uint32_t flags_;

 public:
  enum Flag {
    None_ = 0,
    ObjectFields = 1 << 0,    // shape, class, slots, length etc.
    Element = 1 << 1,         // A Value member of obj->elements or
                              // a typed object.
    UnboxedElement = 1 << 2,  // An unboxed scalar or reference member of
                              // typed object.
    DynamicSlot = 1 << 3,     // A Value member of obj->slots.
    FixedSlot = 1 << 4,       // A Value member of obj->fixedSlots().
    DOMProperty = 1 << 5,     // A DOM property
    WasmGlobalVar = 1 << 6,   // An asm.js/wasm private global var
    WasmHeap = 1 << 7,        // An asm.js/wasm heap load
    WasmHeapMeta = 1 << 8,    // The asm.js/wasm heap base pointer and
                              // bounds check limit, in Tls.
    TypedArrayLengthOrOffset = 1 << 9,  // A typed array's length or byteOffset
    WasmGlobalCell = 1 << 10,           // A wasm global cell
    WasmTableElement = 1 << 11,         // An element of a wasm table
    WasmStackResult = 1 << 12,  // A stack result from the current function

    // JSContext's exception state. This is used on instructions like MThrow
    // that throw exceptions (other than OOM) but have no other side effect, to
    // ensure that they get their own up-to-date resume point. (This resume
    // point will be used when constructing the Baseline frame during exception
    // bailouts.)
    ExceptionState = 1 << 13,

    Last = ExceptionState,
    Any = Last | (Last - 1),

    NumCategories = 14,

    // Indicates load or store.
    Store_ = 1 << 31
  };

  static_assert((1 << NumCategories) - 1 == Any,
                "NumCategories must include all flags present in Any");

  explicit AliasSet(uint32_t flags) : flags_(flags) {}

 public:
  inline bool isNone() const { return flags_ == None_; }
  uint32_t flags() const { return flags_ & Any; }
  inline bool isStore() const { return !!(flags_ & Store_); }
  inline bool isLoad() const { return !isStore() && !isNone(); }
  inline AliasSet operator|(const AliasSet& other) const {
    return AliasSet(flags_ | other.flags_);
  }
  inline AliasSet operator&(const AliasSet& other) const {
    return AliasSet(flags_ & other.flags_);
  }
  static AliasSet None() { return AliasSet(None_); }
  static AliasSet Load(uint32_t flags) {
    MOZ_ASSERT(flags && !(flags & Store_));
    return AliasSet(flags);
  }
  static AliasSet Store(uint32_t flags) {
    MOZ_ASSERT(flags && !(flags & Store_));
    return AliasSet(flags | Store_);
  }
};

typedef Vector<MDefinition*, 6, JitAllocPolicy> MDefinitionVector;
typedef Vector<MInstruction*, 6, JitAllocPolicy> MInstructionVector;
typedef Vector<MDefinition*, 1, JitAllocPolicy> MStoreVector;

class StoreDependency : public TempObject {
  MStoreVector all_;

 public:
  explicit StoreDependency(TempAllocator& alloc) : all_(alloc) {}

  MOZ_MUST_USE bool init(MDefinitionVector& all) {
    if (!all_.appendAll(all)) {
      return false;
    }
    return true;
  }

  MStoreVector& get() { return all_; }
};

// An MDefinition is an SSA name.
class MDefinition : public MNode {
  friend class MBasicBlock;

 public:
  enum class Opcode : uint16_t {
#define DEFINE_OPCODES(op) op,
    MIR_OPCODE_LIST(DEFINE_OPCODES)
#undef DEFINE_OPCODES
  };

 private:
  InlineList<MUse> uses_;  // Use chain.
  uint32_t id_;            // Instruction ID, which after block re-ordering
                           // is sorted within a basic block.
  Opcode op_;              // Opcode.
  uint16_t flags_;         // Bit flags.
  Range* range_;           // Any computed range for this def.
  MIRType resultType_;     // Representation of result type.
  TemporaryTypeSet* resultTypeSet_;  // Optional refinement of the result type.
  union {
    MDefinition*
        loadDependency_;  // Implicit dependency (store, call, etc.) of this
    StoreDependency*
        storeDependency_;  // instruction. Used by alias analysis, GVN and LICM.
    uint32_t virtualRegister_;  // Used by lowering to map definitions to
                                // virtual registers.
  };

  // Track bailouts by storing the current pc in MIR instruction. Also used
  // for profiling and keeping track of what the last known pc was.
  const BytecodeSite* trackedSite_;

 private:
  enum Flag {
    None = 0,
#define DEFINE_FLAG(flag) flag,
    MIR_FLAG_LIST(DEFINE_FLAG)
#undef DEFINE_FLAG
        Total
  };

  bool hasFlags(uint32_t flags) const { return (flags_ & flags) == flags; }
  void removeFlags(uint32_t flags) { flags_ &= ~flags; }
  void setFlags(uint32_t flags) { flags_ |= flags; }

  // Calling isDefinition or isResumePoint on MDefinition is unnecessary.
  bool isDefinition() const = delete;
  bool isResumePoint() const = delete;

 protected:
  void setBlock(MBasicBlock* block) {
    setBlockAndKind(block, Kind::Definition);
  }

  static HashNumber addU32ToHash(HashNumber hash, uint32_t data) {
    return data + (hash << 6) + (hash << 16) - hash;
  }

 public:
  explicit MDefinition(Opcode op)
      : MNode(nullptr, Kind::Definition),
        id_(0),
        op_(op),
        flags_(0),
        range_(nullptr),
        resultType_(MIRType::None),
        resultTypeSet_(nullptr),
        loadDependency_(nullptr),
        trackedSite_(nullptr) {}

  // Copying a definition leaves the list of uses and the block empty.
  explicit MDefinition(const MDefinition& other)
      : MNode(other),
        id_(0),
        op_(other.op_),
        flags_(other.flags_),
        range_(other.range_),
        resultType_(other.resultType_),
        resultTypeSet_(other.resultTypeSet_),
        loadDependency_(other.loadDependency_),
        trackedSite_(other.trackedSite_) {}

  Opcode op() const { return op_; }

#ifdef JS_JITSPEW
  const char* opName() const;
  void printName(GenericPrinter& out) const;
  static void PrintOpcodeName(GenericPrinter& out, Opcode op);
  virtual void printOpcode(GenericPrinter& out) const;
  void dump(GenericPrinter& out) const override;
  void dump() const override;
  void dumpLocation(GenericPrinter& out) const;
  void dumpLocation() const;
#endif

  // For LICM.
  virtual bool neverHoist() const { return false; }

  // Also for LICM. Test whether this definition is likely to be a call, which
  // would clobber all or many of the floating-point registers, such that
  // hoisting floating-point constants out of containing loops isn't likely to
  // be worthwhile.
  virtual bool possiblyCalls() const { return false; }

  MBasicBlock* block() const { return definitionBlock(); }

  void setTrackedSite(const BytecodeSite* site) {
    MOZ_ASSERT(site);
    trackedSite_ = site;
  }
  const BytecodeSite* trackedSite() const { return trackedSite_; }
  jsbytecode* trackedPc() const {
    return trackedSite_ ? trackedSite_->pc() : nullptr;
  }
  InlineScriptTree* trackedTree() const {
    return trackedSite_ ? trackedSite_->tree() : nullptr;
  }

  JSScript* profilerLeaveScript() const {
    return trackedTree()->outermostCaller()->script();
  }

  jsbytecode* profilerLeavePc() const {
    // If this is in a top-level function, use the pc directly.
    if (trackedTree()->isOutermostCaller()) {
      return trackedPc();
    }

    // Walk up the InlineScriptTree chain to find the top-most callPC
    InlineScriptTree* curTree = trackedTree();
    InlineScriptTree* callerTree = curTree->caller();
    while (!callerTree->isOutermostCaller()) {
      curTree = callerTree;
      callerTree = curTree->caller();
    }

    // Return the callPc of the topmost inlined script.
    return curTree->callerPc();
  }

  // Return the range of this value, *before* any bailout checks. Contrast
  // this with the type() method, and the Range constructor which takes an
  // MDefinition*, which describe the value *after* any bailout checks.
  //
  // Warning: Range analysis is removing the bit-operations such as '| 0' at
  // the end of the transformations. Using this function to analyse any
  // operands after the truncate phase of the range analysis will lead to
  // errors. Instead, one should define the collectRangeInfoPreTrunc() to set
  // the right set of flags which are dependent on the range of the inputs.
  Range* range() const {
    MOZ_ASSERT(type() != MIRType::None);
    return range_;
  }
  void setRange(Range* range) {
    MOZ_ASSERT(type() != MIRType::None);
    range_ = range;
  }

  virtual HashNumber valueHash() const;
  virtual bool congruentTo(const MDefinition* ins) const { return false; }
  bool congruentIfOperandsEqual(const MDefinition* ins) const;
  virtual MDefinition* foldsTo(TempAllocator& alloc);
  virtual void analyzeEdgeCasesForward();
  virtual void analyzeEdgeCasesBackward();

  // When a floating-point value is used by nodes which would prefer to
  // recieve integer inputs, we may be able to help by computing our result
  // into an integer directly.
  //
  // A value can be truncated in 4 differents ways:
  //   1. Ignore Infinities (x / 0 --> 0).
  //   2. Ignore overflow (INT_MIN / -1 == (INT_MAX + 1) --> INT_MIN)
  //   3. Ignore negative zeros. (-0 --> 0)
  //   4. Ignore remainder. (3 / 4 --> 0)
  //
  // Indirect truncation is used to represent that we are interested in the
  // truncated result, but only if it can safely flow into operations which
  // are computed modulo 2^32, such as (2) and (3). Infinities are not safe,
  // as they would have absorbed other math operations. Remainders are not
  // safe, as fractions can be scaled up by multiplication.
  //
  // Division is a particularly interesting node here because it covers all 4
  // cases even when its own operands are integers.
  //
  // Note that these enum values are ordered from least value-modifying to
  // most value-modifying, and code relies on this ordering.
  enum TruncateKind {
    // No correction.
    NoTruncate = 0,
    // An integer is desired, but we can't skip bailout checks.
    TruncateAfterBailouts = 1,
    // The value will be truncated after some arithmetic (see above).
    IndirectTruncate = 2,
    // Direct and infallible truncation to int32.
    Truncate = 3
  };

  static const char* TruncateKindString(TruncateKind kind) {
    switch (kind) {
      case NoTruncate:
        return "NoTruncate";
      case TruncateAfterBailouts:
        return "TruncateAfterBailouts";
      case IndirectTruncate:
        return "IndirectTruncate";
      case Truncate:
        return "Truncate";
      default:
        MOZ_CRASH("Unknown truncate kind.");
    }
  }

  // |needTruncation| records the truncation kind of the results, such that it
  // can be used to truncate the operands of this instruction.  If
  // |needTruncation| function returns true, then the |truncate| function is
  // called on the same instruction to mutate the instruction, such as
  // updating the return type, the range and the specialization of the
  // instruction.
  virtual bool needTruncation(TruncateKind kind);
  virtual void truncate();

  // Determine what kind of truncate this node prefers for the operand at the
  // given index.
  virtual TruncateKind operandTruncateKind(size_t index) const;

  // Compute an absolute or symbolic range for the value of this node.
  virtual void computeRange(TempAllocator& alloc) {}

  // Collect information from the pre-truncated ranges.
  virtual void collectRangeInfoPreTrunc() {}

  uint32_t id() const {
    MOZ_ASSERT(block());
    return id_;
  }
  void setId(uint32_t id) { id_ = id; }

#define FLAG_ACCESSOR(flag)                            \
  bool is##flag() const {                              \
    static_assert(Flag::Total <= sizeof(flags_) * 8,   \
                  "Flags should fit in flags_ field"); \
    return hasFlags(1 << flag);                        \
  }                                                    \
  void set##flag() {                                   \
    MOZ_ASSERT(!hasFlags(1 << flag));                  \
    setFlags(1 << flag);                               \
  }                                                    \
  void setNot##flag() {                                \
    MOZ_ASSERT(hasFlags(1 << flag));                   \
    removeFlags(1 << flag);                            \
  }                                                    \
  void set##flag##Unchecked() { setFlags(1 << flag); } \
  void setNot##flag##Unchecked() { removeFlags(1 << flag); }

  MIR_FLAG_LIST(FLAG_ACCESSOR)
#undef FLAG_ACCESSOR

  // Return the type of this value. This may be speculative, and enforced
  // dynamically with the use of bailout checks. If all the bailout checks
  // pass, the value will have this type.
  //
  // Unless this is an MUrsh that has bailouts disabled, which, as a special
  // case, may return a value in (INT32_MAX,UINT32_MAX] even when its type()
  // is MIRType::Int32.
  MIRType type() const { return resultType_; }

  TemporaryTypeSet* resultTypeSet() const { return resultTypeSet_; }
  bool emptyResultTypeSet() const;

  bool mightBeType(MIRType type) const {
    MOZ_ASSERT(type != MIRType::Value);
    MOZ_ASSERT(type != MIRType::ObjectOrNull);

    if (type == this->type()) {
      return true;
    }

    if (this->type() == MIRType::ObjectOrNull) {
      return type == MIRType::Object || type == MIRType::Null;
    }

    if (this->type() == MIRType::Value) {
      return !resultTypeSet() || resultTypeSet()->mightBeMIRType(type);
    }

    return false;
  }

  bool mightBeMagicType() const;

  bool maybeEmulatesUndefined(CompilerConstraintList* constraints);

  // Float32 specialization operations (see big comment in IonAnalysis before
  // the Float32 specialization algorithm).
  virtual bool isFloat32Commutative() const { return false; }
  virtual bool canProduceFloat32() const { return false; }
  virtual bool canConsumeFloat32(MUse* use) const { return false; }
  virtual void trySpecializeFloat32(TempAllocator& alloc) {}
#ifdef DEBUG
  // Used during the pass that checks that Float32 flow into valid MDefinitions
  virtual bool isConsistentFloat32Use(MUse* use) const {
    return type() == MIRType::Float32 || canConsumeFloat32(use);
  }
#endif

  // Returns the beginning of this definition's use chain.
  MUseIterator usesBegin() const { return uses_.begin(); }

  // Returns the end of this definition's use chain.
  MUseIterator usesEnd() const { return uses_.end(); }

  bool canEmitAtUses() const { return !isEmittedAtUses(); }

  // Removes a use at the given position
  void removeUse(MUse* use) { uses_.remove(use); }

#if defined(DEBUG) || defined(JS_JITSPEW)
  // Number of uses of this instruction. This function is only available
  // in DEBUG mode since it requires traversing the list. Most users should
  // use hasUses() or hasOneUse() instead.
  size_t useCount() const;

  // Number of uses of this instruction (only counting MDefinitions, ignoring
  // MResumePoints). This function is only available in DEBUG mode since it
  // requires traversing the list. Most users should use hasUses() or
  // hasOneUse() instead.
  size_t defUseCount() const;
#endif

  // Test whether this MDefinition has exactly one use.
  bool hasOneUse() const;

  // Test whether this MDefinition has exactly one use.
  // (only counting MDefinitions, ignoring MResumePoints)
  bool hasOneDefUse() const;

  // Test whether this MDefinition has at least one use.
  // (only counting MDefinitions, ignoring MResumePoints)
  bool hasDefUses() const;

  // Test whether this MDefinition has at least one non-recovered use.
  // (only counting MDefinitions, ignoring MResumePoints)
  bool hasLiveDefUses() const;

  bool hasUses() const { return !uses_.empty(); }

  void addUse(MUse* use) {
    MOZ_ASSERT(use->producer() == this);
    uses_.pushFront(use);
  }
  void addUseUnchecked(MUse* use) {
    MOZ_ASSERT(use->producer() == this);
    uses_.pushFrontUnchecked(use);
  }
  void replaceUse(MUse* old, MUse* now) {
    MOZ_ASSERT(now->producer() == this);
    uses_.replace(old, now);
  }

  // Replace the current instruction by a dominating instruction |dom| in all
  // uses of the current instruction.
  void replaceAllUsesWith(MDefinition* dom);

  // Like replaceAllUsesWith, but doesn't set UseRemoved on |this|'s operands.
  void justReplaceAllUsesWith(MDefinition* dom);

  // Like justReplaceAllUsesWith, but doesn't replace its own use to the
  // dominating instruction (which would introduce a circular dependency).
  void justReplaceAllUsesWithExcept(MDefinition* dom);

  // Replace the current instruction by an optimized-out constant in all uses
  // of the current instruction. Note, that optimized-out constant should not
  // be observed, and thus they should not flow in any computation.
  MOZ_MUST_USE bool optimizeOutAllUses(TempAllocator& alloc);

  // Replace the current instruction by a dominating instruction |dom| in all
  // instruction, but keep the current instruction for resume point and
  // instruction which are recovered on bailouts.
  void replaceAllLiveUsesWith(MDefinition* dom);

  // Mark this instruction as having replaced all uses of ins, as during GVN,
  // returning false if the replacement should not be performed. For use when
  // GVN eliminates instructions which are not equivalent to one another.
  virtual MOZ_MUST_USE bool updateForReplacement(MDefinition* ins) {
    return true;
  }

  void setVirtualRegister(uint32_t vreg) {
    virtualRegister_ = vreg;
    setLoweredUnchecked();
  }
  uint32_t virtualRegister() const {
    MOZ_ASSERT(isLowered());
    return virtualRegister_;
  }

 public:
  // Opcode testing and casts.
  template <typename MIRType>
  bool is() const {
    return op() == MIRType::classOpcode;
  }
  template <typename MIRType>
  MIRType* to() {
    MOZ_ASSERT(this->is<MIRType>());
    return static_cast<MIRType*>(this);
  }
  template <typename MIRType>
  const MIRType* to() const {
    MOZ_ASSERT(this->is<MIRType>());
    return static_cast<const MIRType*>(this);
  }
#define OPCODE_CASTS(opcode)                                \
  bool is##opcode() const { return this->is<M##opcode>(); } \
  M##opcode* to##opcode() { return this->to<M##opcode>(); } \
  const M##opcode* to##opcode() const { return this->to<M##opcode>(); }
  MIR_OPCODE_LIST(OPCODE_CASTS)
#undef OPCODE_CASTS

  inline MConstant* maybeConstantValue();

  inline MInstruction* toInstruction();
  inline const MInstruction* toInstruction() const;
  bool isInstruction() const { return !isPhi(); }

  virtual bool isControlInstruction() const { return false; }
  inline MControlInstruction* toControlInstruction();

  void setResultType(MIRType type) { resultType_ = type; }
  void setResultTypeSet(TemporaryTypeSet* types) { resultTypeSet_ = types; }
  virtual AliasSet getAliasSet() const {
    // Instructions are effectful by default.
    return AliasSet::Store(AliasSet::Any);
  }

#ifdef DEBUG
  bool hasDefaultAliasSet() const {
    AliasSet set = getAliasSet();
    return set.isStore() && set.flags() == AliasSet::Flag::Any;
  }
#endif

  MDefinition* dependency() const {
    if (getAliasSet().isStore()) {
      return nullptr;
    }
    return loadDependency_;
  }
  void setDependency(MDefinition* dependency) {
    MOZ_ASSERT(!getAliasSet().isStore());
    loadDependency_ = dependency;
  }
  void setStoreDependency(StoreDependency* dependency) {
    MOZ_ASSERT(getAliasSet().isStore());
    storeDependency_ = dependency;
  }
  StoreDependency* storeDependency() {
    MOZ_ASSERT_IF(!getAliasSet().isStore(), !storeDependency_);
    return storeDependency_;
  }
  bool isEffectful() const { return getAliasSet().isStore(); }

#ifdef DEBUG
  virtual bool needsResumePoint() const {
    // Return whether this instruction should have its own resume point.
    return isEffectful();
  }
#endif

  enum class AliasType : uint32_t { NoAlias = 0, MayAlias = 1, MustAlias = 2 };
  virtual AliasType mightAlias(const MDefinition* store) const {
    // Return whether this load may depend on the specified store, given
    // that the alias sets intersect. This may be refined to exclude
    // possible aliasing in cases where alias set flags are too imprecise.
    if (!(getAliasSet().flags() & store->getAliasSet().flags())) {
      return AliasType::NoAlias;
    }
    MOZ_ASSERT(!isEffectful() && store->isEffectful());
    return AliasType::MayAlias;
  }

  virtual bool canRecoverOnBailout() const { return false; }
};

static inline bool SimpleArithOperand(MDefinition* op) {
  return !op->emptyResultTypeSet() && !op->mightBeType(MIRType::Object) &&
         !op->mightBeType(MIRType::String) &&
         !op->mightBeType(MIRType::Symbol) &&
         !op->mightBeType(MIRType::BigInt) && !op->mightBeMagicType();
}

// An MUseDefIterator walks over uses in a definition, skipping any use that is
// not a definition. Items from the use list must not be deleted during
// iteration.
class MUseDefIterator {
  const MDefinition* def_;
  MUseIterator current_;

  MUseIterator search(MUseIterator start) {
    MUseIterator i(start);
    for (; i != def_->usesEnd(); i++) {
      if (i->consumer()->isDefinition()) {
        return i;
      }
    }
    return def_->usesEnd();
  }

 public:
  explicit MUseDefIterator(const MDefinition* def)
      : def_(def), current_(search(def->usesBegin())) {}

  explicit operator bool() const { return current_ != def_->usesEnd(); }
  MUseDefIterator operator++() {
    MOZ_ASSERT(current_ != def_->usesEnd());
    ++current_;
    current_ = search(current_);
    return *this;
  }
  MUseDefIterator operator++(int) {
    MUseDefIterator old(*this);
    operator++();
    return old;
  }
  MUse* use() const { return *current_; }
  MDefinition* def() const { return current_->consumer()->toDefinition(); }
};

#ifdef DEBUG
bool IonCompilationCanUseNurseryPointers();
#endif

// Helper class to check that GC pointers embedded in MIR instructions are in
// in the nursery only when the store buffer has been marked as needing to
// cancel all ion compilations. Otherwise, off-thread Ion compilation and
// nursery GCs can happen in parallel, so it's invalid to store pointers to
// nursery things. There's no need to root these pointers, as GC is suppressed
// during compilation and off-thread compilations are canceled on major GCs.
template <typename T>
class CompilerGCPointer {
  js::gc::Cell* ptr_;

 public:
  explicit CompilerGCPointer(T ptr) : ptr_(ptr) {
    MOZ_ASSERT_IF(IsInsideNursery(ptr), IonCompilationCanUseNurseryPointers());
    MOZ_ASSERT_IF(!CurrentThreadIsIonCompiling(), TlsContext.get()->suppressGC);
  }

  operator T() const { return static_cast<T>(ptr_); }
  T operator->() const { return static_cast<T>(ptr_); }

 private:
  CompilerGCPointer() = delete;
  CompilerGCPointer(const CompilerGCPointer<T>&) = delete;
  CompilerGCPointer<T>& operator=(const CompilerGCPointer<T>&) = delete;
};

using CompilerObject = CompilerGCPointer<JSObject*>;
using CompilerNativeObject = CompilerGCPointer<NativeObject*>;
using CompilerFunction = CompilerGCPointer<JSFunction*>;
using CompilerScript = CompilerGCPointer<JSScript*>;
using CompilerPropertyName = CompilerGCPointer<PropertyName*>;
using CompilerShape = CompilerGCPointer<Shape*>;
using CompilerObjectGroup = CompilerGCPointer<ObjectGroup*>;

class MRootList : public TempObject {
 public:
  using RootVector = Vector<void*, 0, JitAllocPolicy>;

 private:
  mozilla::EnumeratedArray<JS::RootKind, JS::RootKind::Limit,
                           mozilla::Maybe<RootVector>>
      roots_;

  MRootList(const MRootList&) = delete;
  void operator=(const MRootList&) = delete;

 public:
  explicit MRootList(TempAllocator& alloc);

  void trace(JSTracer* trc);

  template <typename T>
  MOZ_MUST_USE bool append(T ptr) {
    if (ptr) {
      return roots_[JS::MapTypeToRootKind<T>::kind]->append(ptr);
    }
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool append(const CompilerGCPointer<T>& ptr) {
    return append(static_cast<T>(ptr));
  }
  MOZ_MUST_USE bool append(const ReceiverGuard& guard) {
    return append(guard.getGroup()) && append(guard.getShape());
  }
};

// An instruction is an SSA name that is inserted into a basic block's IR
// stream.
class MInstruction : public MDefinition, public InlineListNode<MInstruction> {
  MResumePoint* resumePoint_;

 protected:
  // All MInstructions are using the "MFoo::New(alloc)" notation instead of
  // the TempObject new operator. This code redefines the new operator as
  // protected, and delegates to the TempObject new operator. Thus, the
  // following code prevents calls to "new(alloc) MFoo" outside the MFoo
  // members.
  inline void* operator new(size_t nbytes,
                            TempAllocator::Fallible view) noexcept(true) {
    return TempObject::operator new(nbytes, view);
  }
  inline void* operator new(size_t nbytes, TempAllocator& alloc) {
    return TempObject::operator new(nbytes, alloc);
  }
  template <class T>
  inline void* operator new(size_t nbytes, T* pos) {
    return TempObject::operator new(nbytes, pos);
  }

 public:
  explicit MInstruction(Opcode op) : MDefinition(op), resumePoint_(nullptr) {}

  // Copying an instruction leaves the block and resume point as empty.
  explicit MInstruction(const MInstruction& other)
      : MDefinition(other), resumePoint_(nullptr) {}

  // Convenient function used for replacing a load by the value of the store
  // if the types are match, and boxing the value if they do not match.
  MDefinition* foldsToStore(TempAllocator& alloc);

  void setResumePoint(MResumePoint* resumePoint);

  // Used to transfer the resume point to the rewritten instruction.
  void stealResumePoint(MInstruction* ins);
  void moveResumePointAsEntry();
  void clearResumePoint();
  MResumePoint* resumePoint() const { return resumePoint_; }

  // For instructions which can be cloned with new inputs, with all other
  // information being the same. clone() implementations do not need to worry
  // about cloning generic MInstruction/MDefinition state like flags and
  // resume points.
  virtual bool canClone() const { return false; }
  virtual MInstruction* clone(TempAllocator& alloc,
                              const MDefinitionVector& inputs) const {
    MOZ_CRASH();
  }

  // MIR instructions containing GC pointers should override this to append
  // these pointers to the root list.
  virtual bool appendRoots(MRootList& roots) const { return true; }

  // Instructions needing to hook into type analysis should return a
  // TypePolicy.
  virtual const TypePolicy* typePolicy() = 0;
  virtual MIRType typePolicySpecialization() = 0;
};

// Note: GenerateOpcodeFiles.py generates MOpcodes.h based on the
// INSTRUCTION_HEADER* macros.
#define INSTRUCTION_HEADER_WITHOUT_TYPEPOLICY(opcode) \
  static const Opcode classOpcode = Opcode::opcode;   \
  using MThisOpcode = M##opcode;

#define INSTRUCTION_HEADER(opcode)                 \
  INSTRUCTION_HEADER_WITHOUT_TYPEPOLICY(opcode)    \
  virtual const TypePolicy* typePolicy() override; \
  virtual MIRType typePolicySpecialization() override;

#define ALLOW_CLONE(typename)                                                \
  bool canClone() const override { return true; }                            \
  MInstruction* clone(TempAllocator& alloc, const MDefinitionVector& inputs) \
      const override {                                                       \
    MInstruction* res = new (alloc) typename(*this);                         \
    for (size_t i = 0; i < numOperands(); i++)                               \
      res->replaceOperand(i, inputs[i]);                                     \
    return res;                                                              \
  }

// Adds MFoo::New functions which are mirroring the arguments of the
// constructors. Opcodes which are using this macro can be called with a
// TempAllocator, or the fallible version of the TempAllocator.
#define TRIVIAL_NEW_WRAPPERS                                               \
  template <typename... Args>                                              \
  static MThisOpcode* New(TempAllocator& alloc, Args&&... args) {          \
    return new (alloc) MThisOpcode(std::forward<Args>(args)...);           \
  }                                                                        \
  template <typename... Args>                                              \
  static MThisOpcode* New(TempAllocator::Fallible alloc, Args&&... args) { \
    return new (alloc) MThisOpcode(std::forward<Args>(args)...);           \
  }

#define TRIVIAL_NEW_WRAPPERS_WITH_ALLOC                                    \
  template <typename... Args>                                              \
  static MThisOpcode* New(TempAllocator& alloc, Args&&... args) {          \
    return new (alloc) MThisOpcode(alloc, std::forward<Args>(args)...);    \
  }                                                                        \
  template <typename... Args>                                              \
  static MThisOpcode* New(TempAllocator::Fallible alloc, Args&&... args) { \
    return new (alloc) MThisOpcode(alloc, std::forward<Args>(args)...);    \
  }

// These macros are used as a syntactic sugar for writting getOperand
// accessors. They are meant to be used in the body of MIR Instructions as
// follows:
//
//   public:
//     INSTRUCTION_HEADER(Foo)
//     NAMED_OPERANDS((0, lhs), (1, rhs))
//
// The above example defines 2 accessors, one named "lhs" accessing the first
// operand, and a one named "rhs" accessing the second operand.
#define NAMED_OPERAND_ACCESSOR(Index, Name) \
  MDefinition* Name() const { return getOperand(Index); }
#define NAMED_OPERAND_ACCESSOR_APPLY(Args) NAMED_OPERAND_ACCESSOR Args
#define NAMED_OPERANDS(...) \
  MOZ_FOR_EACH(NAMED_OPERAND_ACCESSOR_APPLY, (), (__VA_ARGS__))

template <size_t Arity>
class MAryInstruction : public MInstruction {
  mozilla::Array<MUse, Arity> operands_;

 protected:
  MUse* getUseFor(size_t index) final { return &operands_[index]; }
  const MUse* getUseFor(size_t index) const final { return &operands_[index]; }
  void initOperand(size_t index, MDefinition* operand) {
    operands_[index].init(operand, this);
  }

 public:
  MDefinition* getOperand(size_t index) const final {
    return operands_[index].producer();
  }
  size_t numOperands() const final { return Arity; }
#ifdef DEBUG
  static const size_t staticNumOperands = Arity;
#endif
  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u >= &operands_[0]);
    MOZ_ASSERT(u <= &operands_[numOperands() - 1]);
    return u - &operands_[0];
  }
  void replaceOperand(size_t index, MDefinition* operand) final {
    operands_[index].replaceProducer(operand);
  }

  explicit MAryInstruction(Opcode op) : MInstruction(op) {}

  explicit MAryInstruction(const MAryInstruction<Arity>& other)
      : MInstruction(other) {
    for (int i = 0; i < (int)Arity;
         i++) {  // N.B. use |int| to avoid warnings when Arity == 0
      operands_[i].init(other.operands_[i].producer(), this);
    }
  }
};

class MNullaryInstruction : public MAryInstruction<0>,
                            public NoTypePolicy::Data {
 protected:
  explicit MNullaryInstruction(Opcode op) : MAryInstruction(op) {}

  HashNumber valueHash() const override;
};

class MUnaryInstruction : public MAryInstruction<1> {
 protected:
  MUnaryInstruction(Opcode op, MDefinition* ins) : MAryInstruction(op) {
    initOperand(0, ins);
  }

  HashNumber valueHash() const override;

 public:
  NAMED_OPERANDS((0, input))
};

class MBinaryInstruction : public MAryInstruction<2> {
 protected:
  MBinaryInstruction(Opcode op, MDefinition* left, MDefinition* right)
      : MAryInstruction(op) {
    initOperand(0, left);
    initOperand(1, right);
  }

 public:
  NAMED_OPERANDS((0, lhs), (1, rhs))
  void swapOperands() {
    MDefinition* temp = getOperand(0);
    replaceOperand(0, getOperand(1));
    replaceOperand(1, temp);
  }

 protected:
  HashNumber valueHash() const override;

  bool binaryCongruentTo(const MDefinition* ins) const {
    if (op() != ins->op()) {
      return false;
    }

    if (type() != ins->type()) {
      return false;
    }

    if (isEffectful() || ins->isEffectful()) {
      return false;
    }

    const MDefinition* left = getOperand(0);
    const MDefinition* right = getOperand(1);
    const MDefinition* tmp;

    if (isCommutative() && left->id() > right->id()) {
      tmp = right;
      right = left;
      left = tmp;
    }

    const MBinaryInstruction* bi = static_cast<const MBinaryInstruction*>(ins);
    const MDefinition* insLeft = bi->getOperand(0);
    const MDefinition* insRight = bi->getOperand(1);
    if (isCommutative() && insLeft->id() > insRight->id()) {
      tmp = insRight;
      insRight = insLeft;
      insLeft = tmp;
    }

    return left == insLeft && right == insRight;
  }

 public:
  // Return if the operands to this instruction are both unsigned.
  static bool unsignedOperands(MDefinition* left, MDefinition* right);
  bool unsignedOperands();

  // Replace any wrapping operands with the underlying int32 operands
  // in case of unsigned operands.
  void replaceWithUnsignedOperands();
};

class MTernaryInstruction : public MAryInstruction<3> {
 protected:
  MTernaryInstruction(Opcode op, MDefinition* first, MDefinition* second,
                      MDefinition* third)
      : MAryInstruction(op) {
    initOperand(0, first);
    initOperand(1, second);
    initOperand(2, third);
  }

  HashNumber valueHash() const override;
};

class MQuaternaryInstruction : public MAryInstruction<4> {
 protected:
  MQuaternaryInstruction(Opcode op, MDefinition* first, MDefinition* second,
                         MDefinition* third, MDefinition* fourth)
      : MAryInstruction(op) {
    initOperand(0, first);
    initOperand(1, second);
    initOperand(2, third);
    initOperand(3, fourth);
  }

  HashNumber valueHash() const override;
};

template <class T>
class MVariadicT : public T {
  FixedList<MUse> operands_;

 protected:
  explicit MVariadicT(typename T::Opcode op) : T(op) {}
  MOZ_MUST_USE bool init(TempAllocator& alloc, size_t length) {
    return operands_.init(alloc, length);
  }
  void initOperand(size_t index, MDefinition* operand) {
    // FixedList doesn't initialize its elements, so do an unchecked init.
    operands_[index].initUnchecked(operand, this);
  }
  MUse* getUseFor(size_t index) final { return &operands_[index]; }
  const MUse* getUseFor(size_t index) const final { return &operands_[index]; }

 public:
  // Will assert if called before initialization.
  MDefinition* getOperand(size_t index) const final {
    return operands_[index].producer();
  }
  size_t numOperands() const final { return operands_.length(); }
  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u >= &operands_[0]);
    MOZ_ASSERT(u <= &operands_[numOperands() - 1]);
    return u - &operands_[0];
  }
  void replaceOperand(size_t index, MDefinition* operand) final {
    operands_[index].replaceProducer(operand);
  }
};

using MVariadicInstruction = MVariadicT<MInstruction>;

// Generates an LSnapshot without further effect.
class MStart : public MNullaryInstruction {
  MStart() : MNullaryInstruction(classOpcode) {}

 public:
  INSTRUCTION_HEADER(Start)
  TRIVIAL_NEW_WRAPPERS
};

// Instruction marking on entrypoint for on-stack replacement.
// OSR may occur at loop headers (at JSOp::LoopHead).
// There is at most one MOsrEntry per MIRGraph.
class MOsrEntry : public MNullaryInstruction {
 protected:
  MOsrEntry() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Pointer);
  }

 public:
  INSTRUCTION_HEADER(OsrEntry)
  TRIVIAL_NEW_WRAPPERS
};

// No-op instruction. This cannot be moved or eliminated, and is intended for
// anchoring resume points at arbitrary points in a block.
class MNop : public MNullaryInstruction {
 protected:
  MNop() : MNullaryInstruction(classOpcode) {}

 public:
  INSTRUCTION_HEADER(Nop)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MNop)
};

// Truncation barrier. This is intended for protecting its input against
// follow-up truncation optimizations.
class MLimitedTruncate : public MUnaryInstruction,
                         public ConvertToInt32Policy<0>::Data {
 public:
  TruncateKind truncate_;
  TruncateKind truncateLimit_;

 protected:
  MLimitedTruncate(MDefinition* input, TruncateKind limit)
      : MUnaryInstruction(classOpcode, input),
        truncate_(NoTruncate),
        truncateLimit_(limit) {
    setResultType(MIRType::Int32);
    setResultTypeSet(input->resultTypeSet());
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(LimitedTruncate)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  TruncateKind operandTruncateKind(size_t index) const override;
  TruncateKind truncateKind() const { return truncate_; }
  void setTruncateKind(TruncateKind kind) { truncate_ = kind; }
};

// A constant js::Value.
class MConstant : public MNullaryInstruction {
  struct Payload {
    union {
      bool b;
      int32_t i32;
      int64_t i64;
      float f;
      double d;
      JSString* str;
      JS::Symbol* sym;
      BigInt* bi;
      JSObject* obj;
      uint64_t asBits;
    };
    Payload() : asBits(0) {}
  };

  Payload payload_;

  static_assert(sizeof(Payload) == sizeof(uint64_t),
                "asBits must be big enough for all payload bits");

#ifdef DEBUG
  void assertInitializedPayload() const;
#else
  void assertInitializedPayload() const {}
#endif

 protected:
  MConstant(TempAllocator& alloc, const Value& v,
            CompilerConstraintList* constraints);
  explicit MConstant(JSObject* obj);
  explicit MConstant(float f);
  explicit MConstant(int64_t i);

 public:
  INSTRUCTION_HEADER(Constant)
  static MConstant* New(TempAllocator& alloc, const Value& v,
                        CompilerConstraintList* constraints = nullptr);
  static MConstant* New(TempAllocator::Fallible alloc, const Value& v,
                        CompilerConstraintList* constraints = nullptr);
  static MConstant* New(TempAllocator& alloc, const Value& v, MIRType type);
  static MConstant* NewFloat32(TempAllocator& alloc, double d);
  static MConstant* NewInt64(TempAllocator& alloc, int64_t i);
  static MConstant* NewConstraintlessObject(TempAllocator& alloc, JSObject* v);
  static MConstant* Copy(TempAllocator& alloc, MConstant* src) {
    return new (alloc) MConstant(*src);
  }

  // Try to convert this constant to boolean, similar to js::ToBoolean.
  // Returns false if the type is MIRType::Magic* or MIRType::Object.
  bool MOZ_MUST_USE valueToBoolean(bool* res) const;

  // Like valueToBoolean, but returns the result directly instead of using
  // an outparam. Should not be used if this constant might be a magic value
  // or an object.
  bool valueToBooleanInfallible() const {
    bool res;
    MOZ_ALWAYS_TRUE(valueToBoolean(&res));
    return res;
  }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  HashNumber valueHash() const override;
  bool congruentTo(const MDefinition* ins) const override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool updateForReplacement(MDefinition* def) override {
    MConstant* c = def->toConstant();
    // During constant folding, we don't want to replace a float32
    // value by a double value.
    if (type() == MIRType::Float32) {
      return c->type() == MIRType::Float32;
    }
    if (type() == MIRType::Double) {
      return c->type() != MIRType::Float32;
    }
    return true;
  }

  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;

  bool canProduceFloat32() const override;

  ALLOW_CLONE(MConstant)

  bool equals(const MConstant* other) const {
    assertInitializedPayload();
    return type() == other->type() && payload_.asBits == other->payload_.asBits;
  }

  bool toBoolean() const {
    MOZ_ASSERT(type() == MIRType::Boolean);
    return payload_.b;
  }
  int32_t toInt32() const {
    MOZ_ASSERT(type() == MIRType::Int32);
    return payload_.i32;
  }
  int64_t toInt64() const {
    MOZ_ASSERT(type() == MIRType::Int64);
    return payload_.i64;
  }
  bool isInt32(int32_t i) const {
    return type() == MIRType::Int32 && payload_.i32 == i;
  }
  const double& toDouble() const {
    MOZ_ASSERT(type() == MIRType::Double);
    return payload_.d;
  }
  const float& toFloat32() const {
    MOZ_ASSERT(type() == MIRType::Float32);
    return payload_.f;
  }
  JSString* toString() const {
    MOZ_ASSERT(type() == MIRType::String);
    return payload_.str;
  }
  JS::Symbol* toSymbol() const {
    MOZ_ASSERT(type() == MIRType::Symbol);
    return payload_.sym;
  }
  BigInt* toBigInt() const {
    MOZ_ASSERT(type() == MIRType::BigInt);
    return payload_.bi;
  }
  JSObject& toObject() const {
    MOZ_ASSERT(type() == MIRType::Object);
    return *payload_.obj;
  }
  JSObject* toObjectOrNull() const {
    if (type() == MIRType::Object) {
      return payload_.obj;
    }
    MOZ_ASSERT(type() == MIRType::Null);
    return nullptr;
  }

  bool isTypeRepresentableAsDouble() const {
    return IsTypeRepresentableAsDouble(type());
  }
  double numberToDouble() const {
    MOZ_ASSERT(isTypeRepresentableAsDouble());
    if (type() == MIRType::Int32) {
      return toInt32();
    }
    if (type() == MIRType::Double) {
      return toDouble();
    }
    return toFloat32();
  }

  // Convert this constant to a js::Value. Float32 constants will be stored
  // as DoubleValue and NaNs are canonicalized. Callers must be careful: not
  // all constants can be represented by js::Value (wasm supports int64).
  Value toJSValue() const;

  bool appendRoots(MRootList& roots) const override;
};

class MWasmNullConstant : public MNullaryInstruction {
  explicit MWasmNullConstant() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::RefOrNull);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WasmNullConstant)

  static MWasmNullConstant* New(TempAllocator& alloc) {
    return new (alloc) MWasmNullConstant();
  }

  HashNumber valueHash() const override;
  bool congruentTo(const MDefinition* ins) const override {
    return ins->isWasmNullConstant();
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MWasmNullConstant)
};

// Floating-point value as created by wasm. Just a constant value, used to
// effectively inhibite all the MIR optimizations. This uses the same LIR nodes
// as a MConstant of the same type would.
class MWasmFloatConstant : public MNullaryInstruction {
  union {
    float f32_;
    double f64_;
    uint64_t bits_;
  } u;

  explicit MWasmFloatConstant(MIRType type) : MNullaryInstruction(classOpcode) {
    u.bits_ = 0;
    setResultType(type);
  }

 public:
  INSTRUCTION_HEADER(WasmFloatConstant)

  static MWasmFloatConstant* NewDouble(TempAllocator& alloc, double d) {
    auto* ret = new (alloc) MWasmFloatConstant(MIRType::Double);
    ret->u.f64_ = d;
    return ret;
  }

  static MWasmFloatConstant* NewFloat32(TempAllocator& alloc, float f) {
    auto* ret = new (alloc) MWasmFloatConstant(MIRType::Float32);
    ret->u.f32_ = f;
    return ret;
  }

  HashNumber valueHash() const override;
  bool congruentTo(const MDefinition* ins) const override;
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  const double& toDouble() const {
    MOZ_ASSERT(type() == MIRType::Double);
    return u.f64_;
  }
  const float& toFloat32() const {
    MOZ_ASSERT(type() == MIRType::Float32);
    return u.f32_;
  }
};

// Deep clone a constant JSObject.
class MCloneLiteral : public MUnaryInstruction, public ObjectPolicy<0>::Data {
 protected:
  explicit MCloneLiteral(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(CloneLiteral)
  TRIVIAL_NEW_WRAPPERS
};

class MParameter : public MNullaryInstruction {
  int32_t index_;

  MParameter(int32_t index, TemporaryTypeSet* types)
      : MNullaryInstruction(classOpcode), index_(index) {
    setResultType(MIRType::Value);
    setResultTypeSet(types);
  }

 public:
  INSTRUCTION_HEADER(Parameter)
  TRIVIAL_NEW_WRAPPERS

  static const int32_t THIS_SLOT = -1;
  int32_t index() const { return index_; }
#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif
  HashNumber valueHash() const override;
  bool congruentTo(const MDefinition* ins) const override;
};

class MCallee : public MNullaryInstruction {
 public:
  MCallee() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Object);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Callee)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MIsConstructing : public MNullaryInstruction {
 public:
  MIsConstructing() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsConstructing)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MControlInstruction : public MInstruction {
 protected:
  explicit MControlInstruction(Opcode op) : MInstruction(op) {}

 public:
  virtual size_t numSuccessors() const = 0;
  virtual MBasicBlock* getSuccessor(size_t i) const = 0;
  virtual void replaceSuccessor(size_t i, MBasicBlock* successor) = 0;

  void initSuccessor(size_t i, MBasicBlock* successor) {
    MOZ_ASSERT(!getSuccessor(i));
    replaceSuccessor(i, successor);
  }

  bool isControlInstruction() const override { return true; }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif
};

class MTableSwitch final : public MControlInstruction,
                           public NoFloatPolicy<0>::Data {
  // The successors of the tableswitch
  // - First successor = the default case
  // - Successors 2 and higher = the cases
  Vector<MBasicBlock*, 0, JitAllocPolicy> successors_;
  // Index into successors_ sorted on case index
  Vector<size_t, 0, JitAllocPolicy> cases_;

  MUse operand_;
  int32_t low_;
  int32_t high_;

  void initOperand(size_t index, MDefinition* operand) {
    MOZ_ASSERT(index == 0);
    operand_.init(operand, this);
  }

  MTableSwitch(TempAllocator& alloc, MDefinition* ins, int32_t low,
               int32_t high)
      : MControlInstruction(classOpcode),
        successors_(alloc),
        cases_(alloc),
        low_(low),
        high_(high) {
    initOperand(0, ins);
  }

 protected:
  MUse* getUseFor(size_t index) override {
    MOZ_ASSERT(index == 0);
    return &operand_;
  }

  const MUse* getUseFor(size_t index) const override {
    MOZ_ASSERT(index == 0);
    return &operand_;
  }

 public:
  INSTRUCTION_HEADER(TableSwitch)
  static MTableSwitch* New(TempAllocator& alloc, MDefinition* ins, int32_t low,
                           int32_t high);

  size_t numSuccessors() const override { return successors_.length(); }

  MOZ_MUST_USE bool addSuccessor(MBasicBlock* successor, size_t* index) {
    MOZ_ASSERT(successors_.length() < (size_t)(high_ - low_ + 2));
    MOZ_ASSERT(!successors_.empty());
    *index = successors_.length();
    return successors_.append(successor);
  }

  MBasicBlock* getSuccessor(size_t i) const override {
    MOZ_ASSERT(i < numSuccessors());
    return successors_[i];
  }

  void replaceSuccessor(size_t i, MBasicBlock* successor) override {
    MOZ_ASSERT(i < numSuccessors());
    successors_[i] = successor;
  }

  int32_t low() const { return low_; }

  int32_t high() const { return high_; }

  MBasicBlock* getDefault() const { return getSuccessor(0); }

  MBasicBlock* getCase(size_t i) const { return getSuccessor(cases_[i]); }

  MOZ_MUST_USE bool addDefault(MBasicBlock* block, size_t* index = nullptr) {
    MOZ_ASSERT(successors_.empty());
    if (index) {
      *index = 0;
    }
    return successors_.append(block);
  }

  MOZ_MUST_USE bool addCase(size_t successorIndex) {
    return cases_.append(successorIndex);
  }

  size_t numCases() const { return high() - low() + 1; }

  MDefinition* getOperand(size_t index) const override {
    MOZ_ASSERT(index == 0);
    return operand_.producer();
  }

  size_t numOperands() const override { return 1; }

  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u == getUseFor(0));
    return 0;
  }

  void replaceOperand(size_t index, MDefinition* operand) final {
    MOZ_ASSERT(index == 0);
    operand_.replaceProducer(operand);
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;
};

template <size_t Arity, size_t Successors>
class MAryControlInstruction : public MControlInstruction {
  mozilla::Array<MUse, Arity> operands_;
  mozilla::Array<MBasicBlock*, Successors> successors_;

 protected:
  explicit MAryControlInstruction(Opcode op) : MControlInstruction(op) {}
  void setSuccessor(size_t index, MBasicBlock* successor) {
    successors_[index] = successor;
  }

  MUse* getUseFor(size_t index) final { return &operands_[index]; }
  const MUse* getUseFor(size_t index) const final { return &operands_[index]; }
  void initOperand(size_t index, MDefinition* operand) {
    operands_[index].init(operand, this);
  }

 public:
  MDefinition* getOperand(size_t index) const final {
    return operands_[index].producer();
  }
  size_t numOperands() const final { return Arity; }
  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u >= &operands_[0]);
    MOZ_ASSERT(u <= &operands_[numOperands() - 1]);
    return u - &operands_[0];
  }
  void replaceOperand(size_t index, MDefinition* operand) final {
    operands_[index].replaceProducer(operand);
  }
  size_t numSuccessors() const final { return Successors; }
  MBasicBlock* getSuccessor(size_t i) const final { return successors_[i]; }
  void replaceSuccessor(size_t i, MBasicBlock* succ) final {
    successors_[i] = succ;
  }
};

// Jump to the start of another basic block.
class MGoto : public MAryControlInstruction<0, 1>, public NoTypePolicy::Data {
  explicit MGoto(MBasicBlock* target) : MAryControlInstruction(classOpcode) {
    setSuccessor(0, target);
  }

 public:
  INSTRUCTION_HEADER(Goto)
  static MGoto* New(TempAllocator& alloc, MBasicBlock* target);
  static MGoto* New(TempAllocator::Fallible alloc, MBasicBlock* target);

  // Variant that may patch the target later.
  static MGoto* New(TempAllocator& alloc);

  static const size_t TargetIndex = 0;

  MBasicBlock* target() { return getSuccessor(0); }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

static inline BranchDirection NegateBranchDirection(BranchDirection dir) {
  return (dir == FALSE_BRANCH) ? TRUE_BRANCH : FALSE_BRANCH;
}

// Tests if the input instruction evaluates to true or false, and jumps to the
// start of a corresponding basic block.
class MTest : public MAryControlInstruction<1, 2>, public TestPolicy::Data {
  bool operandMightEmulateUndefined_;

  MTest(MDefinition* ins, MBasicBlock* trueBranch, MBasicBlock* falseBranch)
      : MAryControlInstruction(classOpcode),
        operandMightEmulateUndefined_(true) {
    initOperand(0, ins);
    setSuccessor(0, trueBranch);
    setSuccessor(1, falseBranch);
  }

  // Variant which may patch the ifTrue branch later.
  MTest(MDefinition* ins, MBasicBlock* falseBranch)
      : MTest(ins, nullptr, falseBranch) {}

 public:
  INSTRUCTION_HEADER(Test)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, input))

  static const size_t TrueBranchIndex = 0;

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  MBasicBlock* branchSuccessor(BranchDirection dir) const {
    return (dir == TRUE_BRANCH) ? ifTrue() : ifFalse();
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  // We cache whether our operand might emulate undefined, but we don't want
  // to do that from New() or the constructor, since those can be called on
  // background threads.  So make callers explicitly call it if they want us
  // to check whether the operand might do this.  If this method is never
  // called, we'll assume our operand can emulate undefined.
  void cacheOperandMightEmulateUndefined(CompilerConstraintList* constraints);
  MDefinition* foldsDoubleNegation(TempAllocator& alloc);
  MDefinition* foldsConstant(TempAllocator& alloc);
  MDefinition* foldsTypes(TempAllocator& alloc);
  MDefinition* foldsNeedlessControlFlow(TempAllocator& alloc);
  MDefinition* foldsTo(TempAllocator& alloc) override;
  void filtersUndefinedOrNull(bool trueBranch, MDefinition** subject,
                              bool* filtersUndefined, bool* filtersNull);

  void markNoOperandEmulatesUndefined() {
    operandMightEmulateUndefined_ = false;
  }
  bool operandMightEmulateUndefined() const {
    return operandMightEmulateUndefined_;
  }
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif
};

// Equivalent to MTest(true, successor, fake), except without the foldsTo
// method. This allows IonBuilder to insert fake CFG edges to magically protect
// control flow for try-catch blocks.
class MGotoWithFake : public MAryControlInstruction<0, 2>,
                      public NoTypePolicy::Data {
  MGotoWithFake(MBasicBlock* successor, MBasicBlock* fake)
      : MAryControlInstruction(classOpcode) {
    setSuccessor(0, successor);
    setSuccessor(1, fake);
  }

 public:
  INSTRUCTION_HEADER(GotoWithFake)
  TRIVIAL_NEW_WRAPPERS

  MBasicBlock* target() const { return getSuccessor(0); }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Returns from this function to the previous caller.
class MReturn : public MAryControlInstruction<1, 0>,
                public BoxInputsPolicy::Data {
  explicit MReturn(MDefinition* ins) : MAryControlInstruction(classOpcode) {
    initOperand(0, ins);
  }

 public:
  INSTRUCTION_HEADER(Return)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, input))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MThrow : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MThrow(MDefinition* ins) : MUnaryInstruction(classOpcode, ins) {}

 public:
  INSTRUCTION_HEADER(Throw)
  TRIVIAL_NEW_WRAPPERS

  virtual AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ExceptionState);
  }
  bool possiblyCalls() const override { return true; }
};

// Fabricate a type set containing only the type of the specified object.
TemporaryTypeSet* MakeSingletonTypeSet(TempAllocator& alloc,
                                       CompilerConstraintList* constraints,
                                       JSObject* obj);

TemporaryTypeSet* MakeSingletonTypeSet(TempAllocator& alloc,
                                       CompilerConstraintList* constraints,
                                       ObjectGroup* obj);

MOZ_MUST_USE bool MergeTypes(TempAllocator& alloc, MIRType* ptype,
                             TemporaryTypeSet** ptypeSet, MIRType newType,
                             TemporaryTypeSet* newTypeSet);

bool TypeSetIncludes(TypeSet* types, MIRType input, TypeSet* inputTypes);

bool EqualTypes(MIRType type1, TemporaryTypeSet* typeset1, MIRType type2,
                TemporaryTypeSet* typeset2);

class MNewArray : public MUnaryInstruction, public NoTypePolicy::Data {
 private:
  // Number of elements to allocate for the array.
  uint32_t length_;

  // Heap where the array should be allocated.
  gc::InitialHeap initialHeap_;

  // Whether values written to this array should be converted to double first.
  bool convertDoubleElements_;

  jsbytecode* pc_;

  bool vmCall_;

  MNewArray(TempAllocator& alloc, CompilerConstraintList* constraints,
            uint32_t length, MConstant* templateConst,
            gc::InitialHeap initialHeap, jsbytecode* pc, bool vmCall = false);

 public:
  INSTRUCTION_HEADER(NewArray)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  static MNewArray* NewVM(TempAllocator& alloc,
                          CompilerConstraintList* constraints, uint32_t length,
                          MConstant* templateConst, gc::InitialHeap initialHeap,
                          jsbytecode* pc) {
    return new (alloc) MNewArray(alloc, constraints, length, templateConst,
                                 initialHeap, pc, true);
  }

  uint32_t length() const { return length_; }

  JSObject* templateObject() const {
    return getOperand(0)->toConstant()->toObjectOrNull();
  }

  gc::InitialHeap initialHeap() const { return initialHeap_; }

  jsbytecode* pc() const { return pc_; }

  bool isVMCall() const { return vmCall_; }

  bool convertDoubleElements() const { return convertDoubleElements_; }

  // NewArray is marked as non-effectful because all our allocations are
  // either lazy when we are using "new Array(length)" or bounded by the
  // script or the stack size when we are using "new Array(...)" or "[...]"
  // notations.  So we might have to allocate the array twice if we bail
  // during the computation of the first element of the square braket
  // notation.
  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override {
    // The template object can safely be used in the recover instruction
    // because it can never be mutated by any other function execution.
    return templateObject() != nullptr;
  }
};

class MNewArrayCopyOnWrite : public MUnaryInstruction,
                             public NoTypePolicy::Data {
  gc::InitialHeap initialHeap_;

  MNewArrayCopyOnWrite(TempAllocator& alloc,
                       CompilerConstraintList* constraints,
                       MConstant* templateConst, gc::InitialHeap initialHeap)
      : MUnaryInstruction(classOpcode, templateConst),
        initialHeap_(initialHeap) {
    MOZ_ASSERT(!templateObject()->isSingleton());
    setResultType(MIRType::Object);
    if (!JitOptions.warpBuilder) {
      setResultTypeSet(
          MakeSingletonTypeSet(alloc, constraints, templateObject()));
    }
  }

 public:
  INSTRUCTION_HEADER(NewArrayCopyOnWrite)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  uint32_t length() const { return templateObject()->length(); }

  ArrayObject* templateObject() const {
    return &getOperand(0)->toConstant()->toObject().as<ArrayObject>();
  }

  gc::InitialHeap initialHeap() const { return initialHeap_; }

  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

class MNewArrayDynamicLength : public MUnaryInstruction,
                               public UnboxedInt32Policy<0>::Data {
  CompilerObject templateObject_;
  gc::InitialHeap initialHeap_;

  MNewArrayDynamicLength(TempAllocator& alloc,
                         CompilerConstraintList* constraints,
                         JSObject* templateObject, gc::InitialHeap initialHeap,
                         MDefinition* length)
      : MUnaryInstruction(classOpcode, length),
        templateObject_(templateObject),
        initialHeap_(initialHeap) {
    setGuard();  // Need to throw if length is negative.
    setResultType(MIRType::Object);
    if (!templateObject->isSingleton()) {
      setResultTypeSet(
          MakeSingletonTypeSet(alloc, constraints, templateObject));
    }
  }

 public:
  INSTRUCTION_HEADER(NewArrayDynamicLength)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC
  NAMED_OPERANDS((0, length))

  JSObject* templateObject() const { return templateObject_; }
  gc::InitialHeap initialHeap() const { return initialHeap_; }

  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObject_);
  }
};

class MNewTypedArray : public MUnaryInstruction, public NoTypePolicy::Data {
  gc::InitialHeap initialHeap_;

  MNewTypedArray(TempAllocator& alloc, CompilerConstraintList* constraints,
                 MConstant* templateConst, gc::InitialHeap initialHeap)
      : MUnaryInstruction(classOpcode, templateConst),
        initialHeap_(initialHeap) {
    MOZ_ASSERT(!templateObject()->isSingleton());
    setResultType(MIRType::Object);
    setResultTypeSet(
        MakeSingletonTypeSet(alloc, constraints, templateObject()));
  }

 public:
  INSTRUCTION_HEADER(NewTypedArray)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  TypedArrayObject* templateObject() const {
    return &getOperand(0)->toConstant()->toObject().as<TypedArrayObject>();
  }

  gc::InitialHeap initialHeap() const { return initialHeap_; }

  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

class MNewTypedArrayDynamicLength : public MUnaryInstruction,
                                    public UnboxedInt32Policy<0>::Data {
  CompilerObject templateObject_;
  gc::InitialHeap initialHeap_;

  MNewTypedArrayDynamicLength(TempAllocator& alloc,
                              CompilerConstraintList* constraints,
                              JSObject* templateObject,
                              gc::InitialHeap initialHeap, MDefinition* length)
      : MUnaryInstruction(classOpcode, length),
        templateObject_(templateObject),
        initialHeap_(initialHeap) {
    MOZ_ASSERT(!templateObject->isSingleton());
    setGuard();  // Need to throw if length is negative.
    setResultType(MIRType::Object);
    setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, templateObject));
  }

 public:
  INSTRUCTION_HEADER(NewTypedArrayDynamicLength)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  MDefinition* length() const { return getOperand(0); }
  JSObject* templateObject() const { return templateObject_; }
  gc::InitialHeap initialHeap() const { return initialHeap_; }

  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObject_);
  }
};

// Create a new TypedArray from an Array (or Array-like object) or a TypedArray.
class MNewTypedArrayFromArray : public MUnaryInstruction,
                                public SingleObjectPolicy::Data {
  CompilerObject templateObject_;
  gc::InitialHeap initialHeap_;

  MNewTypedArrayFromArray(TempAllocator& alloc,
                          CompilerConstraintList* constraints,
                          JSObject* templateObject, gc::InitialHeap initialHeap,
                          MDefinition* array)
      : MUnaryInstruction(classOpcode, array),
        templateObject_(templateObject),
        initialHeap_(initialHeap) {
    MOZ_ASSERT(!templateObject->isSingleton());
    setGuard();  // Can throw during construction.
    setResultType(MIRType::Object);
    setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, templateObject));
  }

 public:
  INSTRUCTION_HEADER(NewTypedArrayFromArray)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  MDefinition* array() const { return getOperand(0); }
  JSObject* templateObject() const { return templateObject_; }
  gc::InitialHeap initialHeap() const { return initialHeap_; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObject_);
  }

  bool possiblyCalls() const override { return true; }
};

// Create a new TypedArray from an ArrayBuffer (or SharedArrayBuffer).
class MNewTypedArrayFromArrayBuffer
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>, BoxPolicy<2>>::Data {
  CompilerObject templateObject_;
  gc::InitialHeap initialHeap_;

  MNewTypedArrayFromArrayBuffer(TempAllocator& alloc,
                                CompilerConstraintList* constraints,
                                JSObject* templateObject,
                                gc::InitialHeap initialHeap,
                                MDefinition* arrayBuffer,
                                MDefinition* byteOffset, MDefinition* length)
      : MTernaryInstruction(classOpcode, arrayBuffer, byteOffset, length),
        templateObject_(templateObject),
        initialHeap_(initialHeap) {
    MOZ_ASSERT(!templateObject->isSingleton());
    setGuard();  // Can throw during construction.
    setResultType(MIRType::Object);
    setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, templateObject));
  }

 public:
  INSTRUCTION_HEADER(NewTypedArrayFromArrayBuffer)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  MDefinition* arrayBuffer() const { return getOperand(0); }
  MDefinition* byteOffset() const { return getOperand(1); }
  MDefinition* length() const { return getOperand(2); }
  JSObject* templateObject() const { return templateObject_; }
  gc::InitialHeap initialHeap() const { return initialHeap_; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObject_);
  }

  bool possiblyCalls() const override { return true; }
};

class MNewObject : public MUnaryInstruction, public NoTypePolicy::Data {
 public:
  enum Mode { ObjectLiteral, ObjectCreate };

 private:
  gc::InitialHeap initialHeap_;
  Mode mode_;
  bool vmCall_;

  MNewObject(TempAllocator& alloc, CompilerConstraintList* constraints,
             MConstant* templateConst, gc::InitialHeap initialHeap, Mode mode,
             bool vmCall = false)
      : MUnaryInstruction(classOpcode, templateConst),
        initialHeap_(initialHeap),
        mode_(mode),
        vmCall_(vmCall) {
    MOZ_ASSERT_IF(mode != ObjectLiteral, templateObject());
    setResultType(MIRType::Object);

    if (JSObject* obj = templateObject()) {
      setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, obj));
    }

    // The constant is kept separated in a MConstant, this way we can safely
    // mark it during GC if we recover the object allocation.  Otherwise, by
    // making it emittedAtUses, we do not produce register allocations for
    // it and inline its content inside the code produced by the
    // CodeGenerator.
    if (templateConst->toConstant()->type() == MIRType::Object) {
      templateConst->setEmittedAtUses();
    }
  }

 public:
  INSTRUCTION_HEADER(NewObject)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  static MNewObject* NewVM(TempAllocator& alloc,
                           CompilerConstraintList* constraints,
                           MConstant* templateConst,
                           gc::InitialHeap initialHeap, Mode mode) {
    return new (alloc)
        MNewObject(alloc, constraints, templateConst, initialHeap, mode, true);
  }

  Mode mode() const { return mode_; }

  JSObject* templateObject() const {
    return getOperand(0)->toConstant()->toObjectOrNull();
  }

  gc::InitialHeap initialHeap() const { return initialHeap_; }

  bool isVMCall() const { return vmCall_; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override {
    // The template object can safely be used in the recover instruction
    // because it can never be mutated by any other function execution.
    return templateObject() != nullptr;
  }
};

class MNewIterator : public MUnaryInstruction, public NoTypePolicy::Data {
 public:
  enum Type {
    ArrayIterator,
    StringIterator,
    RegExpStringIterator,
  };

 private:
  Type type_;

  MNewIterator(TempAllocator& alloc, CompilerConstraintList* constraints,
               MConstant* templateConst, Type type)
      : MUnaryInstruction(classOpcode, templateConst), type_(type) {
    setResultType(MIRType::Object);
    setResultTypeSet(
        MakeSingletonTypeSet(alloc, constraints, templateObject()));
    templateConst->setEmittedAtUses();
  }

 public:
  INSTRUCTION_HEADER(NewIterator)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  Type type() const { return type_; }

  JSObject* templateObject() {
    return getOperand(0)->toConstant()->toObjectOrNull();
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

// Represent the content of all slots of an object.  This instruction is not
// lowered and is not used to generate code.
class MObjectState : public MVariadicInstruction,
                     public NoFloatPolicyAfter<1>::Data {
 private:
  uint32_t numSlots_;
  uint32_t numFixedSlots_;

  explicit MObjectState(JSObject* templateObject);
  explicit MObjectState(MObjectState* state);

  MOZ_MUST_USE bool init(TempAllocator& alloc, MDefinition* obj);

  void initSlot(uint32_t slot, MDefinition* def) { initOperand(slot + 1, def); }

 public:
  INSTRUCTION_HEADER(ObjectState)
  NAMED_OPERANDS((0, object))

  // Return the template object of any object creation which can be recovered
  // on bailout.
  static JSObject* templateObjectOf(MDefinition* obj);

  static MObjectState* New(TempAllocator& alloc, MDefinition* obj);
  static MObjectState* Copy(TempAllocator& alloc, MObjectState* state);

  // As we might do read of uninitialized properties, we have to copy the
  // initial values from the template object.
  MOZ_MUST_USE bool initFromTemplateObject(TempAllocator& alloc,
                                           MDefinition* undefinedVal);

  size_t numFixedSlots() const { return numFixedSlots_; }
  size_t numSlots() const { return numSlots_; }

  MDefinition* getSlot(uint32_t slot) const { return getOperand(slot + 1); }
  void setSlot(uint32_t slot, MDefinition* def) {
    replaceOperand(slot + 1, def);
  }

  bool hasFixedSlot(uint32_t slot) const {
    return slot < numSlots() && slot < numFixedSlots();
  }
  MDefinition* getFixedSlot(uint32_t slot) const {
    MOZ_ASSERT(slot < numFixedSlots());
    return getSlot(slot);
  }
  void setFixedSlot(uint32_t slot, MDefinition* def) {
    MOZ_ASSERT(slot < numFixedSlots());
    setSlot(slot, def);
  }

  bool hasDynamicSlot(uint32_t slot) const {
    return numFixedSlots() < numSlots() && slot < numSlots() - numFixedSlots();
  }
  MDefinition* getDynamicSlot(uint32_t slot) const {
    return getSlot(slot + numFixedSlots());
  }
  void setDynamicSlot(uint32_t slot, MDefinition* def) {
    setSlot(slot + numFixedSlots(), def);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

// Represent the contents of all elements of an array.  This instruction is not
// lowered and is not used to generate code.
class MArrayState : public MVariadicInstruction,
                    public NoFloatPolicyAfter<2>::Data {
 private:
  uint32_t numElements_;

  explicit MArrayState(MDefinition* arr);

  MOZ_MUST_USE bool init(TempAllocator& alloc, MDefinition* obj,
                         MDefinition* len);

  void initElement(uint32_t index, MDefinition* def) {
    initOperand(index + 2, def);
  }

 public:
  INSTRUCTION_HEADER(ArrayState)
  NAMED_OPERANDS((0, array), (1, initializedLength))

  static MArrayState* New(TempAllocator& alloc, MDefinition* arr,
                          MDefinition* initLength);
  static MArrayState* Copy(TempAllocator& alloc, MArrayState* state);

  // Initialize values from CopyOnWrite arrays.
  MOZ_MUST_USE bool initFromTemplateObject(TempAllocator& alloc,
                                           MDefinition* undefinedVal);

  void setInitializedLength(MDefinition* def) { replaceOperand(1, def); }

  size_t numElements() const { return numElements_; }

  MDefinition* getElement(uint32_t index) const {
    return getOperand(index + 2);
  }
  void setElement(uint32_t index, MDefinition* def) {
    replaceOperand(index + 2, def);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

// Hold the arguments of an inlined frame. At the moment this class is not
// recovered on bailout as it does not have an implementation and it should
// be inlined at all its uses.
class MArgumentState : public MVariadicInstruction,
                       public NoFloatPolicyAfter<0>::Data {
 private:
  explicit MArgumentState() : MVariadicInstruction(classOpcode) {
    setResultType(MIRType::Object);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ArgumentState)

  static MArgumentState* New(TempAllocator::Fallible view,
                             const MDefinitionVector& args);
  static MArgumentState* Copy(TempAllocator& alloc, MArgumentState* state);

  size_t numElements() const { return numOperands(); }

  MDefinition* getElement(uint32_t index) const { return getOperand(index); }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Setting __proto__ in an object literal.
class MMutateProto : public MBinaryInstruction,
                     public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>>::Data {
 protected:
  MMutateProto(MDefinition* obj, MDefinition* value)
      : MBinaryInstruction(classOpcode, obj, value) {
    setResultType(MIRType::None);
  }

 public:
  INSTRUCTION_HEADER(MutateProto)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getObject), (1, getValue))

  bool possiblyCalls() const override { return true; }
};

class MInitPropGetterSetter
    : public MBinaryInstruction,
      public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>>::Data {
  CompilerPropertyName name_;

  MInitPropGetterSetter(MDefinition* obj, PropertyName* name,
                        MDefinition* value)
      : MBinaryInstruction(classOpcode, obj, value), name_(name) {}

 public:
  INSTRUCTION_HEADER(InitPropGetterSetter)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, value))

  PropertyName* name() const { return name_; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(name_);
  }
};

class MInitElem
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>, BoxPolicy<2>>::Data {
  MInitElem(MDefinition* obj, MDefinition* id, MDefinition* value)
      : MTernaryInstruction(classOpcode, obj, id, value) {
    setResultType(MIRType::None);
  }

 public:
  INSTRUCTION_HEADER(InitElem)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getObject), (1, getId), (2, getValue))

  bool possiblyCalls() const override { return true; }
};

class MInitElemGetterSetter
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>, ObjectPolicy<2>>::Data {
  MInitElemGetterSetter(MDefinition* obj, MDefinition* id, MDefinition* value)
      : MTernaryInstruction(classOpcode, obj, id, value) {}

 public:
  INSTRUCTION_HEADER(InitElemGetterSetter)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, idValue), (2, value))
};

// WrappedFunction wraps a JSFunction so it can safely be used off-thread.
// In particular, a function's flags can be modified on the main thread as
// functions are relazified and delazified, so we must be careful not to access
// these flags off-thread.
class WrappedFunction : public TempObject {
  CompilerFunction fun_;
  uint16_t nargs_;
  bool isNative_ : 1;
  bool isNativeWithJitEntry_ : 1;
  bool isConstructor_ : 1;
  bool isClassConstructor_ : 1;
  bool isSelfHostedBuiltin_ : 1;
  bool isExtended_ : 1;

 public:
  explicit WrappedFunction(JSFunction* fun);
  size_t nargs() const { return nargs_; }

  bool isNative() const { return isNative_; }
  bool isNativeWithJitEntry() const { return isNativeWithJitEntry_; }
  bool isNativeWithCppEntry() const {
    return isNative() && !isNativeWithJitEntry();
  }

  bool isConstructor() const { return isConstructor_; }
  bool isClassConstructor() const { return isClassConstructor_; }
  bool isSelfHostedBuiltin() const { return isSelfHostedBuiltin_; }
  bool isExtended() const { return isExtended_; }

  // fun->native() and fun->jitInfo() can safely be called off-thread: these
  // fields never change.
  JSNative native() const { return fun_->native(); }
  bool hasJitInfo() const { return fun_->hasJitInfo(); }
  const JSJitInfo* jitInfo() const { return fun_->jitInfo(); }

  JSFunction* rawJSFunction() const { return fun_; }

  bool appendRoots(MRootList& roots) const { return roots.append(fun_); }
};

class MCall : public MVariadicInstruction, public CallPolicy::Data {
 private:
  // An MCall uses the MPrepareCall, MDefinition for the function, and
  // MPassArg instructions. They are stored in the same list.
  static const size_t FunctionOperandIndex = 0;
  static const size_t NumNonArgumentOperands = 1;

 protected:
  // Monomorphic cache of single target from TI, or nullptr.
  WrappedFunction* target_;

  // Original value of argc from the bytecode.
  uint32_t numActualArgs_;

  // True if the call is for JSOp::New or JSOp::SuperCall.
  bool construct_ : 1;

  // True if the caller does not use the return value.
  bool ignoresReturnValue_ : 1;

  bool needsArgCheck_ : 1;
  bool needsClassCheck_ : 1;
  bool maybeCrossRealm_ : 1;
  bool needsThisCheck_ : 1;

  MCall(WrappedFunction* target, uint32_t numActualArgs, bool construct,
        bool ignoresReturnValue)
      : MVariadicInstruction(classOpcode),
        target_(target),
        numActualArgs_(numActualArgs),
        construct_(construct),
        ignoresReturnValue_(ignoresReturnValue),
        needsArgCheck_(true),
        needsClassCheck_(true),
        maybeCrossRealm_(true),
        needsThisCheck_(false) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(Call)
  static MCall* New(TempAllocator& alloc, JSFunction* target, size_t maxArgc,
                    size_t numActualArgs, bool construct,
                    bool ignoresReturnValue, bool isDOMCall,
                    DOMObjectKind objectKind);

  void initFunction(MDefinition* func) {
    initOperand(FunctionOperandIndex, func);
  }

  bool needsArgCheck() const { return needsArgCheck_; }
  void disableArgCheck() { needsArgCheck_ = false; }

  bool needsClassCheck() const { return needsClassCheck_; }
  void disableClassCheck() { needsClassCheck_ = false; }

  bool maybeCrossRealm() const { return maybeCrossRealm_; }
  void setNotCrossRealm() { maybeCrossRealm_ = false; }

  bool needsThisCheck() const { return needsThisCheck_; }
  void setNeedsThisCheck() {
    MOZ_ASSERT(construct_);
    needsThisCheck_ = true;
  }

  MDefinition* getFunction() const { return getOperand(FunctionOperandIndex); }
  void replaceFunction(MInstruction* newfunc) {
    replaceOperand(FunctionOperandIndex, newfunc);
  }

  void addArg(size_t argnum, MDefinition* arg);

  MDefinition* getArg(uint32_t index) const {
    return getOperand(NumNonArgumentOperands + index);
  }

  static size_t IndexOfThis() { return NumNonArgumentOperands; }
  static size_t IndexOfArgument(size_t index) {
    return NumNonArgumentOperands + index + 1;  // +1 to skip |this|.
  }
  static size_t IndexOfStackArg(size_t index) {
    return NumNonArgumentOperands + index;
  }

  // For TI-informed monomorphic callsites.
  WrappedFunction* getSingleTarget() const { return target_; }

  bool isConstructing() const { return construct_; }

  bool ignoresReturnValue() const { return ignoresReturnValue_; }

  // The number of stack arguments is the max between the number of formal
  // arguments and the number of actual arguments. The number of stack
  // argument includes the |undefined| padding added in case of underflow.
  // Includes |this|.
  uint32_t numStackArgs() const {
    return numOperands() - NumNonArgumentOperands;
  }

  // Does not include |this|.
  uint32_t numActualArgs() const { return numActualArgs_; }

  bool possiblyCalls() const override { return true; }

  virtual bool isCallDOMNative() const { return false; }

  // A method that can be called to tell the MCall to figure out whether it's
  // movable or not.  This can't be done in the constructor, because it
  // depends on the arguments to the call, and those aren't passed to the
  // constructor but are set up later via addArg.
  virtual void computeMovable() {}

  bool appendRoots(MRootList& roots) const override {
    if (target_) {
      return target_->appendRoots(roots);
    }
    return true;
  }
};

class MCallDOMNative : public MCall {
  // A helper class for MCalls for DOM natives.  Note that this is NOT
  // actually a separate MIR op from MCall, because all sorts of places use
  // isCall() to check for calls and all we really want is to overload a few
  // virtual things from MCall.

  DOMObjectKind objectKind_;

 protected:
  MCallDOMNative(WrappedFunction* target, uint32_t numActualArgs,
                 DOMObjectKind objectKind)
      : MCall(target, numActualArgs, false, false), objectKind_(objectKind) {
    MOZ_ASSERT(getJitInfo()->type() != JSJitInfo::InlinableNative);

    // If our jitinfo is not marked eliminatable, that means that our C++
    // implementation is fallible or that it never wants to be eliminated or
    // that we have no hope of ever doing the sort of argument analysis that
    // would allow us to detemine that we're side-effect-free.  In the
    // latter case we wouldn't get DCEd no matter what, but for the former
    // two cases we have to explicitly say that we can't be DCEd.
    if (!getJitInfo()->isEliminatable) {
      setGuard();
    }
  }

  friend MCall* MCall::New(TempAllocator& alloc, JSFunction* target,
                           size_t maxArgc, size_t numActualArgs, bool construct,
                           bool ignoresReturnValue, bool isDOMCall,
                           DOMObjectKind objectKind);

  const JSJitInfo* getJitInfo() const;

 public:
  DOMObjectKind objectKind() const { return objectKind_; }

  virtual AliasSet getAliasSet() const override;

  virtual bool congruentTo(const MDefinition* ins) const override;

  virtual bool isCallDOMNative() const override { return true; }

  virtual void computeMovable() override;
};

// fun.apply(self, arguments)
class MApplyArgs : public MTernaryInstruction,
                   public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<1>,
                                    BoxPolicy<2>>::Data {
 protected:
  // Monomorphic cache of single target from TI, or nullptr.
  WrappedFunction* target_;
  bool maybeCrossRealm_ = true;
  bool ignoresReturnValue_ = false;

  MApplyArgs(WrappedFunction* target, MDefinition* fun, MDefinition* argc,
             MDefinition* self)
      : MTernaryInstruction(classOpcode, fun, argc, self), target_(target) {
    MOZ_ASSERT(argc->type() == MIRType::Int32);
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(ApplyArgs)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getFunction), (1, getArgc), (2, getThis))

  // For TI-informed monomorphic callsites.
  WrappedFunction* getSingleTarget() const { return target_; }

  bool maybeCrossRealm() const { return maybeCrossRealm_; }
  void setNotCrossRealm() { maybeCrossRealm_ = false; }

  bool ignoresReturnValue() const { return ignoresReturnValue_; }
  void setIgnoresReturnValue() { ignoresReturnValue_ = true; }

  bool isConstructing() const { return false; }

  bool possiblyCalls() const override { return true; }

  bool appendRoots(MRootList& roots) const override {
    if (target_) {
      return target_->appendRoots(roots);
    }
    return true;
  }
};

// fun.apply(fn, array)
class MApplyArray
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>, BoxPolicy<2>>::Data {
 protected:
  // Monomorphic cache of single target from TI, or nullptr.
  WrappedFunction* target_;
  bool maybeCrossRealm_ = true;
  bool ignoresReturnValue_ = false;

  MApplyArray(WrappedFunction* target, MDefinition* fun, MDefinition* elements,
              MDefinition* self)
      : MTernaryInstruction(classOpcode, fun, elements, self), target_(target) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(ApplyArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getFunction), (1, getElements), (2, getThis))

  // For TI-informed monomorphic callsites.
  WrappedFunction* getSingleTarget() const { return target_; }

  bool maybeCrossRealm() const { return maybeCrossRealm_; }
  void setNotCrossRealm() { maybeCrossRealm_ = false; }

  bool ignoresReturnValue() const { return ignoresReturnValue_; }
  void setIgnoresReturnValue() { ignoresReturnValue_ = true; }

  bool isConstructing() const { return false; }

  bool possiblyCalls() const override { return true; }

  bool appendRoots(MRootList& roots) const override {
    if (target_) {
      return target_->appendRoots(roots);
    }
    return true;
  }
};

// |new F(...args)| and |super(...args)|.
class MConstructArray : public MQuaternaryInstruction,
                        public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>,
                                         BoxPolicy<2>, ObjectPolicy<3>>::Data {
  // Monomorphic cache of single target from TI, or nullptr.
  WrappedFunction* target_;
  bool maybeCrossRealm_ = true;

  MConstructArray(WrappedFunction* target, MDefinition* fun,
                  MDefinition* elements, MDefinition* thisValue,
                  MDefinition* newTarget)
      : MQuaternaryInstruction(classOpcode, fun, elements, thisValue,
                               newTarget),
        target_(target) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(ConstructArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getFunction), (1, getElements), (2, getThis),
                 (3, getNewTarget))

  // For TI-informed monomorphic callsites.
  WrappedFunction* getSingleTarget() const { return target_; }

  bool maybeCrossRealm() const { return maybeCrossRealm_; }
  void setNotCrossRealm() { maybeCrossRealm_ = false; }

  bool ignoresReturnValue() const { return false; }
  bool isConstructing() const { return true; }

  bool possiblyCalls() const override { return true; }

  bool appendRoots(MRootList& roots) const override {
    if (target_) {
      return target_->appendRoots(roots);
    }
    return true;
  }
};

class MBail : public MNullaryInstruction {
 protected:
  explicit MBail(BailoutKind kind) : MNullaryInstruction(classOpcode) {
    bailoutKind_ = kind;
    setGuard();
  }

 private:
  BailoutKind bailoutKind_;

 public:
  INSTRUCTION_HEADER(Bail)

  static MBail* New(TempAllocator& alloc, BailoutKind kind) {
    return new (alloc) MBail(kind);
  }
  static MBail* New(TempAllocator& alloc) {
    return new (alloc) MBail(Bailout_Inevitable);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  BailoutKind bailoutKind() const { return bailoutKind_; }
};

class MUnreachable : public MAryControlInstruction<0, 0>,
                     public NoTypePolicy::Data {
  MUnreachable() : MAryControlInstruction(classOpcode) {}

 public:
  INSTRUCTION_HEADER(Unreachable)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// This class serve as a way to force the encoding of a snapshot, even if there
// is no resume point using it.  This is useful to run MAssertRecoveredOnBailout
// assertions.
class MEncodeSnapshot : public MNullaryInstruction {
 protected:
  MEncodeSnapshot() : MNullaryInstruction(classOpcode) { setGuard(); }

 public:
  INSTRUCTION_HEADER(EncodeSnapshot)

  static MEncodeSnapshot* New(TempAllocator& alloc) {
    return new (alloc) MEncodeSnapshot();
  }
};

class MAssertRecoveredOnBailout : public MUnaryInstruction,
                                  public NoTypePolicy::Data {
 protected:
  bool mustBeRecovered_;

  MAssertRecoveredOnBailout(MDefinition* ins, bool mustBeRecovered)
      : MUnaryInstruction(classOpcode, ins), mustBeRecovered_(mustBeRecovered) {
    setResultType(MIRType::Value);
    setRecoveredOnBailout();
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(AssertRecoveredOnBailout)
  TRIVIAL_NEW_WRAPPERS

  // Needed to assert that float32 instructions are correctly recovered.
  bool canConsumeFloat32(MUse* use) const override { return true; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

class MAssertFloat32 : public MUnaryInstruction, public NoTypePolicy::Data {
 protected:
  bool mustBeFloat32_;

  MAssertFloat32(MDefinition* value, bool mustBeFloat32)
      : MUnaryInstruction(classOpcode, value), mustBeFloat32_(mustBeFloat32) {}

 public:
  INSTRUCTION_HEADER(AssertFloat32)
  TRIVIAL_NEW_WRAPPERS

  bool canConsumeFloat32(MUse* use) const override { return true; }

  bool mustBeFloat32() const { return mustBeFloat32_; }
};

class MGetDynamicName
    : public MBinaryInstruction,
      public MixPolicy<ObjectPolicy<0>, ConvertToStringPolicy<1>>::Data {
 protected:
  MGetDynamicName(MDefinition* envChain, MDefinition* name)
      : MBinaryInstruction(classOpcode, envChain, name) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(GetDynamicName)
  NAMED_OPERANDS((0, getEnvironmentChain), (1, getName))

  static MGetDynamicName* New(TempAllocator& alloc, MDefinition* envChain,
                              MDefinition* name) {
    return new (alloc) MGetDynamicName(envChain, name);
  }

  bool possiblyCalls() const override { return true; }
};

class MCallDirectEval
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, StringPolicy<1>, BoxPolicy<2>>::Data {
 protected:
  MCallDirectEval(MDefinition* envChain, MDefinition* string,
                  MDefinition* newTargetValue, jsbytecode* pc)
      : MTernaryInstruction(classOpcode, envChain, string, newTargetValue),
        pc_(pc) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(CallDirectEval)
  NAMED_OPERANDS((0, getEnvironmentChain), (1, getString),
                 (2, getNewTargetValue))

  static MCallDirectEval* New(TempAllocator& alloc, MDefinition* envChain,
                              MDefinition* string, MDefinition* newTargetValue,
                              jsbytecode* pc) {
    return new (alloc) MCallDirectEval(envChain, string, newTargetValue, pc);
  }

  jsbytecode* pc() const { return pc_; }

  bool possiblyCalls() const override { return true; }

 private:
  jsbytecode* pc_;
};

class MCompare : public MBinaryInstruction, public ComparePolicy::Data {
 public:
  enum CompareType {

    // Anything compared to Undefined
    Compare_Undefined,

    // Anything compared to Null
    Compare_Null,

    // Undefined compared to Boolean
    // Null      compared to Boolean
    // Double    compared to Boolean
    // String    compared to Boolean
    // Symbol    compared to Boolean
    // Object    compared to Boolean
    // Value     compared to Boolean
    Compare_Boolean,

    // Int32   compared to Int32
    // Boolean compared to Boolean
    Compare_Int32,
    Compare_Int32MaybeCoerceBoth,
    Compare_Int32MaybeCoerceLHS,
    Compare_Int32MaybeCoerceRHS,

    // Int32 compared as unsigneds
    Compare_UInt32,

    // Int64 compared to Int64.
    Compare_Int64,

    // Int64 compared as unsigneds.
    Compare_UInt64,

    // Double compared to Double
    Compare_Double,

    Compare_DoubleMaybeCoerceLHS,
    Compare_DoubleMaybeCoerceRHS,

    // Float compared to Float
    Compare_Float32,

    // String compared to String
    Compare_String,

    // Symbol compared to Symbol
    Compare_Symbol,

    // Undefined compared to String
    // Null      compared to String
    // Boolean   compared to String
    // Int32     compared to String
    // Double    compared to String
    // Object    compared to String
    // Value     compared to String
    Compare_StrictString,

    // Object compared to Object
    Compare_Object,

    // Wasm Ref/AnyRef/NullRef compared to Ref/AnyRef/NullRef
    Compare_RefOrNull,

    // Compare 2 values bitwise
    Compare_Bitwise,

    // All other possible compares
    Compare_Unknown
  };

 private:
  CompareType compareType_;
  JSOp jsop_;
  bool operandMightEmulateUndefined_;
  bool operandsAreNeverNaN_;

  // When a floating-point comparison is converted to an integer comparison
  // (when range analysis proves it safe), we need to convert the operands
  // to integer as well.
  bool truncateOperands_;

  MCompare(MDefinition* left, MDefinition* right, JSOp jsop)
      : MBinaryInstruction(classOpcode, left, right),
        compareType_(Compare_Unknown),
        jsop_(jsop),
        operandMightEmulateUndefined_(true),
        operandsAreNeverNaN_(false),
        truncateOperands_(false) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

  MCompare(MDefinition* left, MDefinition* right, JSOp jsop,
           CompareType compareType)
      : MCompare(left, right, jsop) {
    MOZ_ASSERT(compareType == Compare_Int32 || compareType == Compare_UInt32 ||
               compareType == Compare_Int64 || compareType == Compare_UInt64 ||
               compareType == Compare_Double ||
               compareType == Compare_Float32 ||
               compareType == Compare_RefOrNull);
    compareType_ = compareType;
    operandMightEmulateUndefined_ = false;
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(Compare)
  TRIVIAL_NEW_WRAPPERS

  MOZ_MUST_USE bool tryFold(bool* result);
  MOZ_MUST_USE bool evaluateConstantOperands(TempAllocator& alloc,
                                             bool* result);
  MDefinition* foldsTo(TempAllocator& alloc) override;
  void filtersUndefinedOrNull(bool trueBranch, MDefinition** subject,
                              bool* filtersUndefined, bool* filtersNull);

  CompareType compareType() const { return compareType_; }
  bool isInt32Comparison() const {
    return compareType() == Compare_Int32 ||
           compareType() == Compare_Int32MaybeCoerceBoth ||
           compareType() == Compare_Int32MaybeCoerceLHS ||
           compareType() == Compare_Int32MaybeCoerceRHS;
  }
  bool isDoubleComparison() const {
    return compareType() == Compare_Double ||
           compareType() == Compare_DoubleMaybeCoerceLHS ||
           compareType() == Compare_DoubleMaybeCoerceRHS;
  }
  bool isFloat32Comparison() const { return compareType() == Compare_Float32; }
  bool isNumericComparison() const {
    return isInt32Comparison() || isDoubleComparison() || isFloat32Comparison();
  }
  void setCompareType(CompareType type) { compareType_ = type; }
  MIRType inputType();

  JSOp jsop() const { return jsop_; }
  void markNoOperandEmulatesUndefined() {
    operandMightEmulateUndefined_ = false;
  }
  bool operandMightEmulateUndefined() const {
    return operandMightEmulateUndefined_;
  }
  bool operandsAreNeverNaN() const { return operandsAreNeverNaN_; }
  AliasSet getAliasSet() const override {
    // Strict equality is never effectful.
    if (jsop_ == JSOp::StrictEq || jsop_ == JSOp::StrictNe) {
      return AliasSet::None();
    }
    if (compareType_ == Compare_Unknown) {
      return AliasSet::Store(AliasSet::Any);
    }
    MOZ_ASSERT(compareType_ <= Compare_Bitwise);
    return AliasSet::None();
  }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif
  void collectRangeInfoPreTrunc() override;

  void trySpecializeFloat32(TempAllocator& alloc) override;
  bool isFloat32Commutative() const override { return true; }
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  TruncateKind operandTruncateKind(size_t index) const override;

  static CompareType determineCompareType(JSOp op, MDefinition* left,
                                          MDefinition* right);
  void cacheOperandMightEmulateUndefined(CompilerConstraintList* constraints);

#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override {
    // Both sides of the compare can be Float32
    return compareType_ == Compare_Float32;
  }
#endif

  ALLOW_CLONE(MCompare)

 protected:
  MOZ_MUST_USE bool tryFoldEqualOperands(bool* result);
  MOZ_MUST_USE bool tryFoldTypeOf(bool* result);

  bool congruentTo(const MDefinition* ins) const override {
    if (!binaryCongruentTo(ins)) {
      return false;
    }
    return compareType() == ins->toCompare()->compareType() &&
           jsop() == ins->toCompare()->jsop();
  }
};

class MSameValue : public MBinaryInstruction, public SameValuePolicy::Data {
  MSameValue(MDefinition* left, MDefinition* right)
      : MBinaryInstruction(classOpcode, left, right) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(SameValue)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MSameValue)
};

// Takes a typed value and returns an untyped value.
class MBox : public MUnaryInstruction, public NoTypePolicy::Data {
  MBox(TempAllocator& alloc, MDefinition* ins);

 public:
  INSTRUCTION_HEADER(Box)
  static MBox* New(TempAllocator& alloc, MDefinition* ins) {
    // Cannot box a box.
    MOZ_ASSERT(ins->type() != MIRType::Value);

    return new (alloc) MBox(alloc, ins);
  }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MBox)
};

// Note: the op may have been inverted during lowering (to put constants in a
// position where they can be immediates), so it is important to use the
// lir->jsop() instead of the mir->jsop() when it is present.
static inline Assembler::Condition JSOpToCondition(
    MCompare::CompareType compareType, JSOp op) {
  bool isSigned = (compareType != MCompare::Compare_UInt32);
  return JSOpToCondition(op, isSigned);
}

// Takes a typed value and checks if it is a certain type. If so, the payload
// is unpacked and returned as that type. Otherwise, it is considered a
// deoptimization.
class MUnbox final : public MUnaryInstruction, public BoxInputsPolicy::Data {
 public:
  enum Mode {
    Fallible,    // Check the type, and deoptimize if unexpected.
    Infallible,  // Type guard is not necessary.
    TypeBarrier  // Guard on the type, and act like a TypeBarrier on failure.
  };

 private:
  Mode mode_;
  BailoutKind bailoutKind_;

  MUnbox(MDefinition* ins, MIRType type, Mode mode, BailoutKind kind,
         TempAllocator& alloc)
      : MUnaryInstruction(classOpcode, ins), mode_(mode) {
    // Only allow unboxing a non MIRType::Value when input and output types
    // don't match. This is often used to force a bailout. Boxing happens
    // during type analysis.
    MOZ_ASSERT_IF(ins->type() != MIRType::Value, type != ins->type());

    MOZ_ASSERT(type == MIRType::Boolean || type == MIRType::Int32 ||
               type == MIRType::Double || type == MIRType::String ||
               type == MIRType::Symbol || type == MIRType::BigInt ||
               type == MIRType::Object);

    TemporaryTypeSet* resultSet = ins->resultTypeSet();
    if (resultSet && type == MIRType::Object) {
      resultSet = resultSet->cloneObjectsOnly(alloc.lifoAlloc());
    }

    setResultType(type);
    setResultTypeSet(resultSet);
    setMovable();

    if (mode_ == TypeBarrier || mode_ == Fallible) {
      setGuard();
    }

    bailoutKind_ = kind;
  }

 public:
  INSTRUCTION_HEADER(Unbox)
  static MUnbox* New(TempAllocator& alloc, MDefinition* ins, MIRType type,
                     Mode mode) {
    // Unless we were given a specific BailoutKind, pick a default based on
    // the type we expect.
    BailoutKind kind;
    switch (type) {
      case MIRType::Boolean:
        kind = Bailout_NonBooleanInput;
        break;
      case MIRType::Int32:
        kind = Bailout_NonInt32Input;
        break;
      case MIRType::Double:
        kind = Bailout_NonNumericInput;  // Int32s are fine too
        break;
      case MIRType::String:
        kind = Bailout_NonStringInput;
        break;
      case MIRType::Symbol:
        kind = Bailout_NonSymbolInput;
        break;
      case MIRType::BigInt:
        kind = Bailout_NonBigIntInput;
        break;
      case MIRType::Object:
        kind = Bailout_NonObjectInput;
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }

    return new (alloc) MUnbox(ins, type, mode, kind, alloc);
  }

  static MUnbox* New(TempAllocator& alloc, MDefinition* ins, MIRType type,
                     Mode mode, BailoutKind kind) {
    return new (alloc) MUnbox(ins, type, mode, kind, alloc);
  }

  Mode mode() const { return mode_; }
  BailoutKind bailoutKind() const {
    // If infallible, no bailout should be generated.
    MOZ_ASSERT(fallible());
    return bailoutKind_;
  }
  bool fallible() const { return mode() != Infallible; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isUnbox() || ins->toUnbox()->mode() != mode()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }
#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif
  void makeInfallible() {
    // Should only be called if we're already Infallible or TypeBarrier
    MOZ_ASSERT(mode() != Fallible);
    mode_ = Infallible;
  }

  ALLOW_CLONE(MUnbox)
};

class MGuardObject : public MUnaryInstruction, public SingleObjectPolicy::Data {
  explicit MGuardObject(MDefinition* ins)
      : MUnaryInstruction(classOpcode, ins) {
    setGuard();
    setMovable();
    setResultType(MIRType::Object);
    setResultTypeSet(ins->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(GuardObject)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MGuardString : public MUnaryInstruction, public StringPolicy<0>::Data {
  explicit MGuardString(MDefinition* ins)
      : MUnaryInstruction(classOpcode, ins) {
    setGuard();
    setMovable();
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(GuardString)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MPolyInlineGuard : public MUnaryInstruction,
                         public SingleObjectPolicy::Data {
  explicit MPolyInlineGuard(MDefinition* ins)
      : MUnaryInstruction(classOpcode, ins) {
    setGuard();
    setResultType(MIRType::Object);
    setResultTypeSet(ins->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(PolyInlineGuard)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MAssertRange : public MUnaryInstruction, public NoTypePolicy::Data {
  // This is the range checked by the assertion. Don't confuse this with the
  // range_ member or the range() accessor. Since MAssertRange doesn't return
  // a value, it doesn't use those.
  const Range* assertedRange_;

  MAssertRange(MDefinition* ins, const Range* assertedRange)
      : MUnaryInstruction(classOpcode, ins), assertedRange_(assertedRange) {
    setGuard();
    setResultType(MIRType::None);
  }

 public:
  INSTRUCTION_HEADER(AssertRange)
  TRIVIAL_NEW_WRAPPERS

  const Range* assertedRange() const { return assertedRange_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif
};

// Caller-side allocation of |this| for |new|:
// Given a templateobject, construct |this| for JSOp::New.
// Not used for JSOp::SuperCall, because Baseline doesn't attach template
// objects for super calls.
class MCreateThisWithTemplate : public MUnaryInstruction,
                                public NoTypePolicy::Data {
  gc::InitialHeap initialHeap_;

  MCreateThisWithTemplate(TempAllocator& alloc,
                          CompilerConstraintList* constraints,
                          MConstant* templateConst, gc::InitialHeap initialHeap)
      : MUnaryInstruction(classOpcode, templateConst),
        initialHeap_(initialHeap) {
    setResultType(MIRType::Object);
    setResultTypeSet(
        MakeSingletonTypeSet(alloc, constraints, templateObject()));
  }

 public:
  INSTRUCTION_HEADER(CreateThisWithTemplate)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  // Template for |this|, provided by TI.
  JSObject* templateObject() const {
    return &getOperand(0)->toConstant()->toObject();
  }

  gc::InitialHeap initialHeap() const { return initialHeap_; }

  // Although creation of |this| modifies global state, it is safely repeatable.
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override;
};

// Caller-side allocation of |this| for |new|:
// Given a prototype operand, construct |this| for JSOp::New or JSOp::SuperCall.
class MCreateThisWithProto : public MTernaryInstruction,
                             public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>,
                                              ObjectPolicy<2>>::Data {
  MCreateThisWithProto(MDefinition* callee, MDefinition* newTarget,
                       MDefinition* prototype)
      : MTernaryInstruction(classOpcode, callee, newTarget, prototype) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(CreateThisWithProto)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getCallee), (1, getNewTarget), (2, getPrototype))

  // Although creation of |this| modifies global state, it is safely repeatable.
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool possiblyCalls() const override { return true; }
};

// Caller-side allocation of |this| for |new|:
// Constructs |this| when possible, else MagicValue(JS_IS_CONSTRUCTING).
class MCreateThis : public MBinaryInstruction,
                    public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>>::Data {
  explicit MCreateThis(MDefinition* callee, MDefinition* newTarget)
      : MBinaryInstruction(classOpcode, callee, newTarget) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(CreateThis)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getCallee), (1, getNewTarget))

  // Performs a property read from |newTarget| iff |newTarget| is a JSFunction
  // with an own |.prototype| property.
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::Any);
  }
  bool possiblyCalls() const override { return true; }
};

// Eager initialization of arguments object.
class MCreateArgumentsObject : public MUnaryInstruction,
                               public ObjectPolicy<0>::Data {
  CompilerGCPointer<ArgumentsObject*> templateObj_;

  MCreateArgumentsObject(MDefinition* callObj, ArgumentsObject* templateObj)
      : MUnaryInstruction(classOpcode, callObj), templateObj_(templateObj) {
    setResultType(MIRType::Object);
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(CreateArgumentsObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getCallObject))

  ArgumentsObject* templateObject() const { return templateObj_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool possiblyCalls() const override { return true; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObj_);
  }
};

class MGetArgumentsObjectArg : public MUnaryInstruction,
                               public ObjectPolicy<0>::Data {
  size_t argno_;

  MGetArgumentsObjectArg(MDefinition* argsObject, size_t argno)
      : MUnaryInstruction(classOpcode, argsObject), argno_(argno) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(GetArgumentsObjectArg)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getArgsObject))

  size_t argno() const { return argno_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::Any);
  }
};

class MSetArgumentsObjectArg
    : public MBinaryInstruction,
      public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>>::Data {
  size_t argno_;

  MSetArgumentsObjectArg(MDefinition* argsObj, size_t argno, MDefinition* value)
      : MBinaryInstruction(classOpcode, argsObj, value), argno_(argno) {}

 public:
  INSTRUCTION_HEADER(SetArgumentsObjectArg)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getArgsObject), (1, getValue))

  size_t argno() const { return argno_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::Any);
  }
};

// Given a MIRType::Value A and a MIRType::Object B:
// If the Value may be safely unboxed to an Object, return Object(A).
// Otherwise, return B.
// Used to implement return behavior for inlined constructors.
class MReturnFromCtor : public MBinaryInstruction,
                        public MixPolicy<BoxPolicy<0>, ObjectPolicy<1>>::Data {
  MReturnFromCtor(MDefinition* value, MDefinition* object)
      : MBinaryInstruction(classOpcode, value, object) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(ReturnFromCtor)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getValue), (1, getObject))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MToFPInstruction : public MUnaryInstruction, public ToDoublePolicy::Data {
 public:
  // Types of values which can be converted.
  enum ConversionKind {
    NonStringPrimitives,
    NonNullNonStringPrimitives,
    NumbersOnly
  };

 private:
  ConversionKind conversion_;

 protected:
  MToFPInstruction(Opcode op, MDefinition* def,
                   ConversionKind conversion = NonStringPrimitives)
      : MUnaryInstruction(op, def), conversion_(conversion) {}

 public:
  ConversionKind conversion() const { return conversion_; }
};

// Converts a primitive (either typed or untyped) to a double. If the input is
// not primitive at runtime, a bailout occurs.
class MToDouble : public MToFPInstruction {
 private:
  TruncateKind implicitTruncate_;

  explicit MToDouble(MDefinition* def,
                     ConversionKind conversion = NonStringPrimitives)
      : MToFPInstruction(classOpcode, def, conversion),
        implicitTruncate_(NoTruncate) {
    setResultType(MIRType::Double);
    setMovable();

    // An object might have "valueOf", which means it is effectful.
    // ToNumber(symbol) and ToNumber(bigint) throw.
    if (def->mightBeType(MIRType::Object) ||
        def->mightBeType(MIRType::Symbol) ||
        def->mightBeType(MIRType::BigInt)) {
      setGuard();
    }
  }

 public:
  INSTRUCTION_HEADER(ToDouble)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isToDouble() || ins->toToDouble()->conversion() != conversion()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  TruncateKind operandTruncateKind(size_t index) const override;

#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif

  TruncateKind truncateKind() const { return implicitTruncate_; }
  void setTruncateKind(TruncateKind kind) {
    implicitTruncate_ = std::max(implicitTruncate_, kind);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override {
    if (input()->type() == MIRType::Value) {
      return false;
    }
    if (input()->type() == MIRType::Symbol) {
      return false;
    }
    if (input()->type() == MIRType::BigInt) {
      return false;
    }

    return true;
  }

  ALLOW_CLONE(MToDouble)
};

// Converts a primitive (either typed or untyped) to a float32. If the input is
// not primitive at runtime, a bailout occurs.
class MToFloat32 : public MToFPInstruction {
 protected:
  bool mustPreserveNaN_;

  explicit MToFloat32(MDefinition* def,
                      ConversionKind conversion = NonStringPrimitives)
      : MToFPInstruction(classOpcode, def, conversion),
        mustPreserveNaN_(false) {
    setResultType(MIRType::Float32);
    setMovable();

    // An object might have "valueOf", which means it is effectful.
    // ToNumber(symbol) and ToNumber(BigInt) throw.
    if (def->mightBeType(MIRType::Object) ||
        def->mightBeType(MIRType::Symbol) ||
        def->mightBeType(MIRType::BigInt)) {
      setGuard();
    }
  }

  explicit MToFloat32(MDefinition* def, bool mustPreserveNaN)
      : MToFloat32(def) {
    mustPreserveNaN_ = mustPreserveNaN;
  }

 public:
  INSTRUCTION_HEADER(ToFloat32)
  TRIVIAL_NEW_WRAPPERS

  virtual MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!congruentIfOperandsEqual(ins)) {
      return false;
    }
    auto* other = ins->toToFloat32();
    return other->conversion() == conversion() &&
           other->mustPreserveNaN_ == mustPreserveNaN_;
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  void computeRange(TempAllocator& alloc) override;

  bool canConsumeFloat32(MUse* use) const override { return true; }
  bool canProduceFloat32() const override { return true; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MToFloat32)
};

// Converts a uint32 to a double (coming from wasm).
class MWasmUnsignedToDouble : public MUnaryInstruction,
                              public NoTypePolicy::Data {
  explicit MWasmUnsignedToDouble(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::Double);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WasmUnsignedToDouble)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Converts a uint32 to a float32 (coming from wasm).
class MWasmUnsignedToFloat32 : public MUnaryInstruction,
                               public NoTypePolicy::Data {
  explicit MWasmUnsignedToFloat32(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::Float32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WasmUnsignedToFloat32)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool canProduceFloat32() const override { return true; }
};

class MWrapInt64ToInt32 : public MUnaryInstruction, public NoTypePolicy::Data {
  bool bottomHalf_;

  explicit MWrapInt64ToInt32(MDefinition* def, bool bottomHalf = true)
      : MUnaryInstruction(classOpcode, def), bottomHalf_(bottomHalf) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WrapInt64ToInt32)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isWrapInt64ToInt32()) {
      return false;
    }
    if (ins->toWrapInt64ToInt32()->bottomHalf() != bottomHalf()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool bottomHalf() const { return bottomHalf_; }
};

class MExtendInt32ToInt64 : public MUnaryInstruction,
                            public NoTypePolicy::Data {
  bool isUnsigned_;

  MExtendInt32ToInt64(MDefinition* def, bool isUnsigned)
      : MUnaryInstruction(classOpcode, def), isUnsigned_(isUnsigned) {
    setResultType(MIRType::Int64);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ExtendInt32ToInt64)
  TRIVIAL_NEW_WRAPPERS

  bool isUnsigned() const { return isUnsigned_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isExtendInt32ToInt64()) {
      return false;
    }
    if (ins->toExtendInt32ToInt64()->isUnsigned_ != isUnsigned_) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MWasmTruncateToInt64 : public MUnaryInstruction,
                             public NoTypePolicy::Data {
  TruncFlags flags_;
  wasm::BytecodeOffset bytecodeOffset_;

  MWasmTruncateToInt64(MDefinition* def, TruncFlags flags,
                       wasm::BytecodeOffset bytecodeOffset)
      : MUnaryInstruction(classOpcode, def),
        flags_(flags),
        bytecodeOffset_(bytecodeOffset) {
    setResultType(MIRType::Int64);
    setGuard();  // neither removable nor movable because of possible
                 // side-effects.
  }

 public:
  INSTRUCTION_HEADER(WasmTruncateToInt64)
  TRIVIAL_NEW_WRAPPERS

  bool isUnsigned() const { return flags_ & TRUNC_UNSIGNED; }
  bool isSaturating() const { return flags_ & TRUNC_SATURATING; }
  TruncFlags flags() const { return flags_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           ins->toWasmTruncateToInt64()->flags() == flags_;
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Truncate a value to an int32, with wasm semantics: this will trap when the
// value is out of range.
class MWasmTruncateToInt32 : public MUnaryInstruction,
                             public NoTypePolicy::Data {
  TruncFlags flags_;
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmTruncateToInt32(MDefinition* def, TruncFlags flags,
                                wasm::BytecodeOffset bytecodeOffset)
      : MUnaryInstruction(classOpcode, def),
        flags_(flags),
        bytecodeOffset_(bytecodeOffset) {
    setResultType(MIRType::Int32);
    setGuard();  // neither removable nor movable because of possible
                 // side-effects.
  }

 public:
  INSTRUCTION_HEADER(WasmTruncateToInt32)
  TRIVIAL_NEW_WRAPPERS

  bool isUnsigned() const { return flags_ & TRUNC_UNSIGNED; }
  bool isSaturating() const { return flags_ & TRUNC_SATURATING; }
  TruncFlags flags() const { return flags_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           ins->toWasmTruncateToInt32()->flags() == flags_;
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Store a JS Value that can't be represented as an AnyRef pointer into an
// object that holds the value (opaquely) as such a pointer.
class MWasmBoxValue : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MWasmBoxValue(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::RefOrNull);
  }

 public:
  INSTRUCTION_HEADER(WasmBoxValue)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MWasmAnyRefFromJSObject : public MUnaryInstruction,
                                public NoTypePolicy::Data {
  explicit MWasmAnyRefFromJSObject(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::RefOrNull);
  }

 public:
  INSTRUCTION_HEADER(WasmAnyRefFromJSObject)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MInt64ToFloatingPoint : public MUnaryInstruction,
                              public NoTypePolicy::Data {
  bool isUnsigned_;
  wasm::BytecodeOffset bytecodeOffset_;

  MInt64ToFloatingPoint(MDefinition* def, MIRType type,
                        wasm::BytecodeOffset bytecodeOffset, bool isUnsigned)
      : MUnaryInstruction(classOpcode, def),
        isUnsigned_(isUnsigned),
        bytecodeOffset_(bytecodeOffset) {
    MOZ_ASSERT(IsFloatingPointType(type));
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Int64ToFloatingPoint)
  TRIVIAL_NEW_WRAPPERS

  bool isUnsigned() const { return isUnsigned_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isInt64ToFloatingPoint()) {
      return false;
    }
    if (ins->toInt64ToFloatingPoint()->isUnsigned_ != isUnsigned_) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Takes a boxed Value and returns a Value containing either a Number or a
// BigInt.  Usually this will be the value itself, but it may be an object that
// has a @@toPrimitive, valueOf, or toString method.
class MToNumeric : public MUnaryInstruction, public BoxInputsPolicy::Data {
  MToNumeric(MDefinition* arg, TemporaryTypeSet* types)
      : MUnaryInstruction(classOpcode, arg) {
    if (!JitOptions.warpBuilder) {
      // Note: unlike IonBuilder, WarpBuilder always adds MToNumeric and relies
      // on specializing it later on.
      MOZ_ASSERT(!IsNumericType(arg->type()),
                 "Unboxable definitions don't need ToNumeric");
    }
    setResultType(MIRType::Value);
    // Although `types' is always Int32|Double|BigInt, we have to compute it in
    // IonBuilder to know whether emitting an MToNumeric is needed, so we just
    // pass it through as an argument instead of recomputing it here.
    setResultTypeSet(types);
    setGuard();
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ToNumeric)
  TRIVIAL_NEW_WRAPPERS

  void computeRange(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  MDefinition* foldsTo(TempAllocator& alloc) override;

  ALLOW_CLONE(MToNumeric)
};

// Applies ECMA's ToNumber on a primitive (either typed or untyped) and expects
// the result to be precisely representable as an Int32, otherwise bails.
//
// If the input is not primitive at runtime, a bailout occurs. If the input
// cannot be converted to an int32 without loss (i.e. 5.5 or undefined) then a
// bailout occurs.
class MToNumberInt32 : public MUnaryInstruction, public ToInt32Policy::Data {
  bool canBeNegativeZero_;
  IntConversionInputKind conversion_;

  explicit MToNumberInt32(MDefinition* def, IntConversionInputKind conversion =
                                                IntConversionInputKind::Any)
      : MUnaryInstruction(classOpcode, def),
        canBeNegativeZero_(true),
        conversion_(conversion) {
    setResultType(MIRType::Int32);
    setMovable();

    // An object might have "valueOf", which means it is effectful.
    // ToNumber(symbol) and ToNumber(BigInt) throw.
    if (def->mightBeType(MIRType::Object) ||
        def->mightBeType(MIRType::Symbol) ||
        def->mightBeType(MIRType::BigInt)) {
      setGuard();
    }
  }

 public:
  INSTRUCTION_HEADER(ToNumberInt32)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  // this only has backwards information flow.
  void analyzeEdgeCasesBackward() override;

  bool canBeNegativeZero() const { return canBeNegativeZero_; }
  void setCanBeNegativeZero(bool negativeZero) {
    canBeNegativeZero_ = negativeZero;
  }

  IntConversionInputKind conversion() const { return conversion_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isToNumberInt32() ||
        ins->toToNumberInt32()->conversion() != conversion()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;
  void collectRangeInfoPreTrunc() override;

#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif

  ALLOW_CLONE(MToNumberInt32)
};

// Applies ECMA's ToInteger on a primitive (either typed or untyped) and expects
// the result to be precisely representable as an Int32, otherwise bails.
//
// NB: Negative zero doesn't lead to a bailout, but instead will be treated the
// same as positive zero for this operation.
//
// If the input is not primitive at runtime, a bailout occurs. If the input
// cannot be converted to an int32 without loss (i.e. 2e10 or Infinity) then a
// bailout occurs.
class MToIntegerInt32 : public MUnaryInstruction, public ToInt32Policy::Data {
  explicit MToIntegerInt32(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::Int32);
    setMovable();

    // An object might have "valueOf", which means it is effectful.
    // ToInteger(symbol) and ToInteger(BigInt) throw.
    if (def->mightBeType(MIRType::Object) ||
        def->mightBeType(MIRType::Symbol) ||
        def->mightBeType(MIRType::BigInt)) {
      setGuard();
    }
  }

 public:
  INSTRUCTION_HEADER(ToIntegerInt32)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;

#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif

  ALLOW_CLONE(MToIntegerInt32)
};

// Converts a value or typed input to a truncated int32, for use with bitwise
// operations. This is an infallible ValueToECMAInt32.
class MTruncateToInt32 : public MUnaryInstruction, public ToInt32Policy::Data {
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MTruncateToInt32(
      MDefinition* def,
      wasm::BytecodeOffset bytecodeOffset = wasm::BytecodeOffset())
      : MUnaryInstruction(classOpcode, def), bytecodeOffset_(bytecodeOffset) {
    setResultType(MIRType::Int32);
    setMovable();

    // An object might have "valueOf", which means it is effectful.
    // ToInt32(symbol) and ToInt32(BigInt) throw.
    if (def->mightBeType(MIRType::Object) ||
        def->mightBeType(MIRType::Symbol) ||
        def->mightBeType(MIRType::BigInt)) {
      setGuard();
    }
  }

 public:
  INSTRUCTION_HEADER(TruncateToInt32)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  void computeRange(TempAllocator& alloc) override;
  TruncateKind operandTruncateKind(size_t index) const override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override {
    return input()->type() < MIRType::Symbol;
  }

  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  ALLOW_CLONE(MTruncateToInt32)
};

// Converts any type to a string
class MToString : public MUnaryInstruction, public ToStringPolicy::Data {
 public:
  // MToString has two modes for handling of object/symbol arguments: if the
  // to-string conversion happens as part of another opcode, we have to bail out
  // to Baseline. If the conversion is for a stand-alone JSOp we can support
  // side-effects.
  enum class SideEffectHandling { Bailout, Supported };

 private:
  SideEffectHandling sideEffects_;
  bool mightHaveSideEffects_ = false;

  MToString(MDefinition* def, SideEffectHandling sideEffects)
      : MUnaryInstruction(classOpcode, def), sideEffects_(sideEffects) {
    setResultType(MIRType::String);

    if (JitOptions.warpBuilder) {
      mightHaveSideEffects_ = true;
    } else {
      if (input()->mightBeType(MIRType::Object) ||
          input()->mightBeType(MIRType::Symbol)) {
        mightHaveSideEffects_ = true;
      }

      // If this instruction is not effectful, mark it as movable and set the
      // Guard flag if needed. If the operation is effectful it won't be
      // optimized anyway so there's no need to set any flags.
      if (!isEffectful()) {
        setMovable();
        // Objects might override toString; Symbol throws. We bailout in those
        // cases and run side-effects in baseline instead.
        if (mightHaveSideEffects_) {
          setGuard();
        }
      }
    }
  }

 public:
  INSTRUCTION_HEADER(ToString)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isToString()) {
      return false;
    }
    if (sideEffects_ != ins->toToString()->sideEffects_) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override {
    if (supportSideEffects() && mightHaveSideEffects_) {
      return AliasSet::Store(AliasSet::Any);
    }
    return AliasSet::None();
  }

  bool mightHaveSideEffects() const { return mightHaveSideEffects_; }

  bool supportSideEffects() const {
    return sideEffects_ == SideEffectHandling::Supported;
  }

  bool needsSnapshot() const {
    return sideEffects_ == SideEffectHandling::Bailout && mightHaveSideEffects_;
  }

  ALLOW_CLONE(MToString)
};

// Converts any type to an object, throwing on null or undefined.
class MToObject : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MToObject(MDefinition* def) : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::Object);
    setGuard();  // Throws on null or undefined.
  }

 public:
  INSTRUCTION_HEADER(ToObject)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MToObject)
};

// Converts any type to an object or null value, throwing on undefined.
class MToObjectOrNull : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MToObjectOrNull(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::ObjectOrNull);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ToObjectOrNull)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MToObjectOrNull)
};

class MBitNot : public MUnaryInstruction, public BitwisePolicy::Data {
 protected:
  explicit MBitNot(MDefinition* input) : MUnaryInstruction(classOpcode, input) {
    specialization_ = MIRType::Int32;
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(BitNot)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MBitNot)
};

class MTypeOf : public MUnaryInstruction, public BoxInputsPolicy::Data {
  MIRType inputType_;
  bool inputMaybeCallableOrEmulatesUndefined_;

  MTypeOf(MDefinition* def, MIRType inputType)
      : MUnaryInstruction(classOpcode, def),
        inputType_(inputType),
        inputMaybeCallableOrEmulatesUndefined_(true) {
    setResultType(MIRType::String);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(TypeOf)
  TRIVIAL_NEW_WRAPPERS

  MIRType inputType() const { return inputType_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  void cacheInputMaybeCallableOrEmulatesUndefined(
      CompilerConstraintList* constraints);

  bool inputMaybeCallableOrEmulatesUndefined() const {
    return inputMaybeCallableOrEmulatesUndefined_;
  }
  void markInputNotCallableOrEmulatesUndefined() {
    inputMaybeCallableOrEmulatesUndefined_ = false;
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isTypeOf()) {
      return false;
    }
    if (inputType() != ins->toTypeOf()->inputType()) {
      return false;
    }
    if (inputMaybeCallableOrEmulatesUndefined() !=
        ins->toTypeOf()->inputMaybeCallableOrEmulatesUndefined()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

class MToAsyncIter : public MBinaryInstruction,
                     public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>>::Data {
  explicit MToAsyncIter(MDefinition* iterator, MDefinition* nextMethod)
      : MBinaryInstruction(classOpcode, iterator, nextMethod) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(ToAsyncIter)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, getIterator), (1, getNextMethod))
};

class MToId : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MToId(MDefinition* index) : MUnaryInstruction(classOpcode, index) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(ToId)
  TRIVIAL_NEW_WRAPPERS
};

class MBinaryBitwiseInstruction : public MBinaryInstruction,
                                  public BitwisePolicy::Data {
 protected:
  MBinaryBitwiseInstruction(Opcode op, MDefinition* left, MDefinition* right,
                            MIRType type)
      : MBinaryInstruction(op, left, right),
        maskMatchesLeftRange(false),
        maskMatchesRightRange(false) {
    MOZ_ASSERT(type == MIRType::Int32 || type == MIRType::Int64 ||
               (isUrsh() && type == MIRType::Double));
    specialization_ = type;
    setResultType(type);
    setMovable();
  }

  bool maskMatchesLeftRange;
  bool maskMatchesRightRange;

 public:
  MDefinition* foldsTo(TempAllocator& alloc) override;
  MDefinition* foldUnnecessaryBitop();
  virtual MDefinition* foldIfZero(size_t operand) = 0;
  virtual MDefinition* foldIfNegOne(size_t operand) = 0;
  virtual MDefinition* foldIfEqual() = 0;
  virtual MDefinition* foldIfAllBitsSet(size_t operand) = 0;
  void collectRangeInfoPreTrunc() override;

  bool congruentTo(const MDefinition* ins) const override {
    return binaryCongruentTo(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  TruncateKind operandTruncateKind(size_t index) const override;
};

class MBitAnd : public MBinaryBitwiseInstruction {
  MBitAnd(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryBitwiseInstruction(classOpcode, left, right, type) {
    setCommutative();
  }

 public:
  INSTRUCTION_HEADER(BitAnd)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldIfZero(size_t operand) override {
    return getOperand(operand);  // 0 & x => 0;
  }
  MDefinition* foldIfNegOne(size_t operand) override {
    return getOperand(1 - operand);  // x & -1 => x
  }
  MDefinition* foldIfEqual() override {
    return getOperand(0);  // x & x => x;
  }
  MDefinition* foldIfAllBitsSet(size_t operand) override {
    // e.g. for uint16: x & 0xffff => x;
    return getOperand(1 - operand);
  }
  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MBitAnd)
};

class MBitOr : public MBinaryBitwiseInstruction {
  MBitOr(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryBitwiseInstruction(classOpcode, left, right, type) {
    setCommutative();
  }

 public:
  INSTRUCTION_HEADER(BitOr)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldIfZero(size_t operand) override {
    return getOperand(1 -
                      operand);  // 0 | x => x, so if ith is 0, return (1-i)th
  }
  MDefinition* foldIfNegOne(size_t operand) override {
    return getOperand(operand);  // x | -1 => -1
  }
  MDefinition* foldIfEqual() override {
    return getOperand(0);  // x | x => x
  }
  MDefinition* foldIfAllBitsSet(size_t operand) override { return this; }
  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MBitOr)
};

class MBitXor : public MBinaryBitwiseInstruction {
  MBitXor(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryBitwiseInstruction(classOpcode, left, right, type) {
    setCommutative();
  }

 public:
  INSTRUCTION_HEADER(BitXor)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldIfZero(size_t operand) override {
    return getOperand(1 - operand);  // 0 ^ x => x
  }
  MDefinition* foldIfNegOne(size_t operand) override { return this; }
  MDefinition* foldIfEqual() override { return this; }
  MDefinition* foldIfAllBitsSet(size_t operand) override { return this; }
  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MBitXor)
};

class MShiftInstruction : public MBinaryBitwiseInstruction {
 protected:
  MShiftInstruction(Opcode op, MDefinition* left, MDefinition* right,
                    MIRType type)
      : MBinaryBitwiseInstruction(op, left, right, type) {}

 public:
  MDefinition* foldIfNegOne(size_t operand) override { return this; }
  MDefinition* foldIfEqual() override { return this; }
  MDefinition* foldIfAllBitsSet(size_t operand) override { return this; }
};

class MLsh : public MShiftInstruction {
  MLsh(MDefinition* left, MDefinition* right, MIRType type)
      : MShiftInstruction(classOpcode, left, right, type) {}

 public:
  INSTRUCTION_HEADER(Lsh)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldIfZero(size_t operand) override {
    // 0 << x => 0
    // x << 0 => x
    return getOperand(0);
  }

  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MLsh)
};

class MRsh : public MShiftInstruction {
  MRsh(MDefinition* left, MDefinition* right, MIRType type)
      : MShiftInstruction(classOpcode, left, right, type) {}

 public:
  INSTRUCTION_HEADER(Rsh)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldIfZero(size_t operand) override {
    // 0 >> x => 0
    // x >> 0 => x
    return getOperand(0);
  }
  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  ALLOW_CLONE(MRsh)
};

class MUrsh : public MShiftInstruction {
  bool bailoutsDisabled_;

  MUrsh(MDefinition* left, MDefinition* right, MIRType type)
      : MShiftInstruction(classOpcode, left, right, type),
        bailoutsDisabled_(false) {}

 public:
  INSTRUCTION_HEADER(Ursh)
  TRIVIAL_NEW_WRAPPERS

  static MUrsh* NewWasm(TempAllocator& alloc, MDefinition* left,
                        MDefinition* right, MIRType type);

  MDefinition* foldIfZero(size_t operand) override {
    // 0 >>> x => 0
    if (operand == 0) {
      return getOperand(0);
    }

    return this;
  }

  bool bailoutsDisabled() const { return bailoutsDisabled_; }

  bool fallible() const;

  void computeRange(TempAllocator& alloc) override;
  void collectRangeInfoPreTrunc() override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MUrsh)
};

class MSignExtendInt32 : public MUnaryInstruction, public NoTypePolicy::Data {
 public:
  enum Mode { Byte, Half };

 private:
  Mode mode_;

  MSignExtendInt32(MDefinition* op, Mode mode)
      : MUnaryInstruction(classOpcode, op), mode_(mode) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(SignExtendInt32)
  TRIVIAL_NEW_WRAPPERS

  Mode mode() const { return mode_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!congruentIfOperandsEqual(ins)) {
      return false;
    }
    return ins->isSignExtendInt32() && ins->toSignExtendInt32()->mode_ == mode_;
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MSignExtendInt32)
};

class MSignExtendInt64 : public MUnaryInstruction, public NoTypePolicy::Data {
 public:
  enum Mode { Byte, Half, Word };

 private:
  Mode mode_;

  MSignExtendInt64(MDefinition* op, Mode mode)
      : MUnaryInstruction(classOpcode, op), mode_(mode) {
    setResultType(MIRType::Int64);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(SignExtendInt64)
  TRIVIAL_NEW_WRAPPERS

  Mode mode() const { return mode_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!congruentIfOperandsEqual(ins)) {
      return false;
    }
    return ins->isSignExtendInt64() && ins->toSignExtendInt64()->mode_ == mode_;
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MSignExtendInt64)
};

class MBinaryArithInstruction : public MBinaryInstruction,
                                public ArithPolicy::Data {
  // Implicit truncate flag is set by the truncate backward range analysis
  // optimization phase, and by wasm pre-processing. It is used in
  // NeedNegativeZeroCheck to check if the result of a multiplication needs to
  // produce -0 double value, and for avoiding overflow checks.

  // This optimization happens when the multiplication cannot be truncated
  // even if all uses are truncating its result, such as when the range
  // analysis detect a precision loss in the multiplication.
  TruncateKind implicitTruncate_;

  // Whether we must preserve NaN semantics, and in particular not fold
  // (x op id) or (id op x) to x, or replace a division by a multiply of the
  // exact reciprocal.
  bool mustPreserveNaN_;

 public:
  MBinaryArithInstruction(Opcode op, MDefinition* left, MDefinition* right,
                          MIRType type)
      : MBinaryInstruction(op, left, right),
        implicitTruncate_(NoTruncate),
        mustPreserveNaN_(false) {
    MOZ_ASSERT(IsNumberType(type));
    specialization_ = type;
    setResultType(type);
    setMovable();
  }

  static MBinaryArithInstruction* New(TempAllocator& alloc, Opcode op,
                                      MDefinition* left, MDefinition* right,
                                      MIRType specialization);

  bool constantDoubleResult(TempAllocator& alloc);

  void setMustPreserveNaN(bool b) { mustPreserveNaN_ = b; }
  bool mustPreserveNaN() const { return mustPreserveNaN_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  virtual double getIdentity() = 0;

  void setSpecialization(MIRType type) {
    MOZ_ASSERT(IsNumberType(type));
    specialization_ = type;
    setResultType(type);
  }

  virtual void trySpecializeFloat32(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    if (!binaryCongruentTo(ins)) {
      return false;
    }
    const auto* other = static_cast<const MBinaryArithInstruction*>(ins);
    return other->mustPreserveNaN_ == mustPreserveNaN_;
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool isTruncated() const { return implicitTruncate_ == Truncate; }
  TruncateKind truncateKind() const { return implicitTruncate_; }
  void setTruncateKind(TruncateKind kind) {
    implicitTruncate_ = std::max(implicitTruncate_, kind);
  }
};

class MMinMax : public MBinaryInstruction, public ArithPolicy::Data {
  bool isMax_;

  MMinMax(MDefinition* left, MDefinition* right, MIRType type, bool isMax)
      : MBinaryInstruction(classOpcode, left, right), isMax_(isMax) {
    MOZ_ASSERT(IsNumberType(type));
    setResultType(type);
    setMovable();
    specialization_ = type;
  }

 public:
  INSTRUCTION_HEADER(MinMax)
  TRIVIAL_NEW_WRAPPERS

  static MMinMax* NewWasm(TempAllocator& alloc, MDefinition* left,
                          MDefinition* right, MIRType type, bool isMax) {
    return New(alloc, left, right, type, isMax);
  }

  bool isMax() const { return isMax_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!congruentIfOperandsEqual(ins)) {
      return false;
    }
    const MMinMax* other = ins->toMinMax();
    return other->isMax() == isMax();
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  MDefinition* foldsTo(TempAllocator& alloc) override;
  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;

  ALLOW_CLONE(MMinMax)
};

class MAbs : public MUnaryInstruction, public ArithPolicy::Data {
  bool implicitTruncate_;

  MAbs(MDefinition* num, MIRType type)
      : MUnaryInstruction(classOpcode, num), implicitTruncate_(false) {
    MOZ_ASSERT(IsNumberType(type));
    setResultType(type);
    setMovable();
    specialization_ = type;
  }

 public:
  INSTRUCTION_HEADER(Abs)
  TRIVIAL_NEW_WRAPPERS

  static MAbs* NewWasm(TempAllocator& alloc, MDefinition* num, MIRType type) {
    auto* ins = new (alloc) MAbs(num, type);
    if (type == MIRType::Int32) {
      ins->implicitTruncate_ = true;
    }
    return ins;
  }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  bool fallible() const;

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;
  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MAbs)
};

class MClz : public MUnaryInstruction, public BitwisePolicy::Data {
  bool operandIsNeverZero_;

  explicit MClz(MDefinition* num, MIRType type)
      : MUnaryInstruction(classOpcode, num), operandIsNeverZero_(false) {
    MOZ_ASSERT(IsIntType(type));
    MOZ_ASSERT(IsNumberType(num->type()));
    specialization_ = type;
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Clz)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, num))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool operandIsNeverZero() const { return operandIsNeverZero_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  void computeRange(TempAllocator& alloc) override;
  void collectRangeInfoPreTrunc() override;
};

class MCtz : public MUnaryInstruction, public BitwisePolicy::Data {
  bool operandIsNeverZero_;

  explicit MCtz(MDefinition* num, MIRType type)
      : MUnaryInstruction(classOpcode, num), operandIsNeverZero_(false) {
    MOZ_ASSERT(IsIntType(type));
    MOZ_ASSERT(IsNumberType(num->type()));
    specialization_ = type;
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Ctz)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, num))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool operandIsNeverZero() const { return operandIsNeverZero_; }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  void computeRange(TempAllocator& alloc) override;
  void collectRangeInfoPreTrunc() override;
};

class MPopcnt : public MUnaryInstruction, public BitwisePolicy::Data {
  explicit MPopcnt(MDefinition* num, MIRType type)
      : MUnaryInstruction(classOpcode, num) {
    MOZ_ASSERT(IsNumberType(num->type()));
    MOZ_ASSERT(IsIntType(type));
    specialization_ = type;
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Popcnt)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, num))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  void computeRange(TempAllocator& alloc) override;
};

// Inline implementation of Math.sqrt().
class MSqrt : public MUnaryInstruction, public FloatingPointPolicy<0>::Data {
  MSqrt(MDefinition* num, MIRType type) : MUnaryInstruction(classOpcode, num) {
    setResultType(type);
    specialization_ = type;
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Sqrt)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;

  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MSqrt)
};

class MCopySign : public MBinaryInstruction, public NoTypePolicy::Data {
  MCopySign(MDefinition* lhs, MDefinition* rhs, MIRType type)
      : MBinaryInstruction(classOpcode, lhs, rhs) {
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(CopySign)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MCopySign)
};

// Inline implementation of atan2 (arctangent of y/x).
class MAtan2 : public MBinaryInstruction,
               public MixPolicy<DoublePolicy<0>, DoublePolicy<1>>::Data {
  MAtan2(MDefinition* y, MDefinition* x)
      : MBinaryInstruction(classOpcode, y, x) {
    setResultType(MIRType::Double);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Atan2)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, y), (1, x))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool possiblyCalls() const override { return true; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MAtan2)
};

// Inline implementation of Math.hypot().
class MHypot : public MVariadicInstruction, public AllDoublePolicy::Data {
  MHypot() : MVariadicInstruction(classOpcode) {
    setResultType(MIRType::Double);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Hypot)
  static MHypot* New(TempAllocator& alloc, const MDefinitionVector& vector);

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool possiblyCalls() const override { return true; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  bool canClone() const override { return true; }

  MInstruction* clone(TempAllocator& alloc,
                      const MDefinitionVector& inputs) const override {
    return MHypot::New(alloc, inputs);
  }
};

// Inline implementation of Math.pow().
class MPow : public MBinaryInstruction, public PowPolicy::Data {
  MPow(MDefinition* input, MDefinition* power, MIRType powerType)
      : MBinaryInstruction(classOpcode, input, power) {
    MOZ_ASSERT(powerType == MIRType::Double || powerType == MIRType::Int32);
    specialization_ = powerType;
    setResultType(MIRType::Double);
    setMovable();
  }

  // Helpers for `foldsTo`
  MDefinition* foldsConstant(TempAllocator& alloc);
  MDefinition* foldsConstantPower(TempAllocator& alloc);

 public:
  INSTRUCTION_HEADER(Pow)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* input() const { return lhs(); }
  MDefinition* power() const { return rhs(); }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool possiblyCalls() const override { return true; }
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  ALLOW_CLONE(MPow)
};

// Inline implementation of Math.pow(x, 0.5), which subtly differs from
// Math.sqrt(x).
class MPowHalf : public MUnaryInstruction, public DoublePolicy<0>::Data {
  bool operandIsNeverNegativeInfinity_;
  bool operandIsNeverNegativeZero_;
  bool operandIsNeverNaN_;

  explicit MPowHalf(MDefinition* input)
      : MUnaryInstruction(classOpcode, input),
        operandIsNeverNegativeInfinity_(false),
        operandIsNeverNegativeZero_(false),
        operandIsNeverNaN_(false) {
    setResultType(MIRType::Double);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(PowHalf)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  bool operandIsNeverNegativeInfinity() const {
    return operandIsNeverNegativeInfinity_;
  }
  bool operandIsNeverNegativeZero() const {
    return operandIsNeverNegativeZero_;
  }
  bool operandIsNeverNaN() const { return operandIsNeverNaN_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void collectRangeInfoPreTrunc() override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MPowHalf)
};

// Inline implementation of Math.random().
class MRandom : public MNullaryInstruction {
  MRandom() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Double);
  }

 public:
  INSTRUCTION_HEADER(Random)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool possiblyCalls() const override { return true; }

  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;

  bool canRecoverOnBailout() const override {
#ifdef JS_MORE_DETERMINISTIC
    return false;
#else
    return true;
#endif
  }

  ALLOW_CLONE(MRandom)
};

class MSign : public MUnaryInstruction, public SignPolicy::Data {
 private:
  MSign(MDefinition* input, MIRType resultType)
      : MUnaryInstruction(classOpcode, input) {
    MOZ_ASSERT(IsNumberType(input->type()));
    MOZ_ASSERT(resultType == MIRType::Int32 || resultType == MIRType::Double);
    specialization_ = input->type();
    setResultType(resultType);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Sign)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MSign)
};

class MMathFunction : public MUnaryInstruction,
                      public FloatingPointPolicy<0>::Data {
 public:
  enum Function {
    Log,
    Sin,
    Cos,
    Exp,
    Tan,
    ACos,
    ASin,
    ATan,
    Log10,
    Log2,
    Log1P,
    ExpM1,
    CosH,
    SinH,
    TanH,
    ACosH,
    ASinH,
    ATanH,
    Trunc,
    Cbrt,
    Floor,
    Ceil,
    Round
  };

 private:
  Function function_;

  // A nullptr cache means this function will neither access nor update the
  // cache.
  MMathFunction(MDefinition* input, Function function)
      : MUnaryInstruction(classOpcode, input), function_(function) {
    setResultType(MIRType::Double);
    specialization_ = MIRType::Double;
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(MathFunction)
  TRIVIAL_NEW_WRAPPERS

  Function function() const { return function_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isMathFunction()) {
      return false;
    }
    if (ins->toMathFunction()->function() != function()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool possiblyCalls() const override { return true; }

  MDefinition* foldsTo(TempAllocator& alloc) override;

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  static const char* FunctionName(Function function);

  bool isFloat32Commutative() const override {
    return function_ == Floor || function_ == Ceil || function_ == Round ||
           function_ == Trunc;
  }
  void trySpecializeFloat32(TempAllocator& alloc) override;
  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override {
    switch (function_) {
      case Sin:
      case Log:
      case Ceil:
      case Floor:
      case Round:
      case Trunc:
        return true;
      default:
        return false;
    }
  }

  ALLOW_CLONE(MMathFunction)
};

class MAdd : public MBinaryArithInstruction {
  MAdd(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryArithInstruction(classOpcode, left, right, type) {}

  MAdd(MDefinition* left, MDefinition* right, TruncateKind truncateKind)
      : MAdd(left, right, MIRType::Int32) {
    setTruncateKind(truncateKind);
    setCommutative();
  }

 public:
  INSTRUCTION_HEADER(Add)
  TRIVIAL_NEW_WRAPPERS

  static MAdd* NewWasm(TempAllocator& alloc, MDefinition* left,
                       MDefinition* right, MIRType type) {
    auto* ret = new (alloc) MAdd(left, right, type);
    if (type == MIRType::Int32) {
      ret->setTruncateKind(Truncate);
      ret->setCommutative();
    }
    return ret;
  }

  bool isFloat32Commutative() const override { return true; }

  double getIdentity() override { return 0; }

  bool fallible() const;
  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  TruncateKind operandTruncateKind(size_t index) const override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MAdd)
};

class MSub : public MBinaryArithInstruction {
  MSub(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryArithInstruction(classOpcode, left, right, type) {}

 public:
  INSTRUCTION_HEADER(Sub)
  TRIVIAL_NEW_WRAPPERS

  static MSub* NewWasm(TempAllocator& alloc, MDefinition* left,
                       MDefinition* right, MIRType type, bool mustPreserveNaN) {
    auto* ret = new (alloc) MSub(left, right, type);
    ret->setMustPreserveNaN(mustPreserveNaN);
    if (type == MIRType::Int32) {
      ret->setTruncateKind(Truncate);
    }
    return ret;
  }

  double getIdentity() override { return 0; }

  bool isFloat32Commutative() const override { return true; }

  bool fallible() const;
  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  TruncateKind operandTruncateKind(size_t index) const override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MSub)
};

class MMul : public MBinaryArithInstruction {
 public:
  enum Mode { Normal, Integer };

 private:
  // Annotation the result could be a negative zero
  // and we need to guard this during execution.
  bool canBeNegativeZero_;

  Mode mode_;

  MMul(MDefinition* left, MDefinition* right, MIRType type, Mode mode)
      : MBinaryArithInstruction(classOpcode, left, right, type),
        canBeNegativeZero_(true),
        mode_(mode) {
    if (mode == Integer) {
      // This implements the required behavior for Math.imul, which
      // can never fail and always truncates its output to int32.
      canBeNegativeZero_ = false;
      setTruncateKind(Truncate);
      setCommutative();
    }
    MOZ_ASSERT_IF(mode != Integer, mode == Normal);
  }

 public:
  INSTRUCTION_HEADER(Mul)

  static MMul* New(TempAllocator& alloc, MDefinition* left, MDefinition* right,
                   MIRType type, Mode mode = Normal) {
    return new (alloc) MMul(left, right, type, mode);
  }
  static MMul* NewWasm(TempAllocator& alloc, MDefinition* left,
                       MDefinition* right, MIRType type, Mode mode,
                       bool mustPreserveNaN) {
    auto* ret = new (alloc) MMul(left, right, type, mode);
    ret->setMustPreserveNaN(mustPreserveNaN);
    return ret;
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  void analyzeEdgeCasesForward() override;
  void analyzeEdgeCasesBackward() override;
  void collectRangeInfoPreTrunc() override;

  double getIdentity() override { return 1; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isMul()) {
      return false;
    }

    const MMul* mul = ins->toMul();
    if (canBeNegativeZero_ != mul->canBeNegativeZero()) {
      return false;
    }

    if (mode_ != mul->mode()) {
      return false;
    }

    if (mustPreserveNaN() != mul->mustPreserveNaN()) {
      return false;
    }

    return binaryCongruentTo(ins);
  }

  bool canOverflow() const;

  bool canBeNegativeZero() const { return canBeNegativeZero_; }
  void setCanBeNegativeZero(bool negativeZero) {
    canBeNegativeZero_ = negativeZero;
  }

  MOZ_MUST_USE bool updateForReplacement(MDefinition* ins) override;

  bool fallible() const { return canBeNegativeZero_ || canOverflow(); }

  bool isFloat32Commutative() const override { return true; }

  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  TruncateKind operandTruncateKind(size_t index) const override;

  Mode mode() const { return mode_; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MMul)
};

class MDiv : public MBinaryArithInstruction {
  bool canBeNegativeZero_;
  bool canBeNegativeOverflow_;
  bool canBeDivideByZero_;
  bool canBeNegativeDividend_;
  bool unsigned_;  // If false, signedness will be derived from operands
  bool trapOnError_;
  wasm::BytecodeOffset bytecodeOffset_;

  MDiv(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryArithInstruction(classOpcode, left, right, type),
        canBeNegativeZero_(true),
        canBeNegativeOverflow_(true),
        canBeDivideByZero_(true),
        canBeNegativeDividend_(true),
        unsigned_(false),
        trapOnError_(false) {}

 public:
  INSTRUCTION_HEADER(Div)

  static MDiv* New(TempAllocator& alloc, MDefinition* left, MDefinition* right,
                   MIRType type) {
    return new (alloc) MDiv(left, right, type);
  }
  static MDiv* New(TempAllocator& alloc, MDefinition* left, MDefinition* right,
                   MIRType type, bool unsignd, bool trapOnError = false,
                   wasm::BytecodeOffset bytecodeOffset = wasm::BytecodeOffset(),
                   bool mustPreserveNaN = false) {
    auto* div = new (alloc) MDiv(left, right, type);
    div->unsigned_ = unsignd;
    div->trapOnError_ = trapOnError;
    div->bytecodeOffset_ = bytecodeOffset;
    if (trapOnError) {
      div->setGuard();  // not removable because of possible side-effects.
      div->setNotMovable();
    }
    div->setMustPreserveNaN(mustPreserveNaN);
    if (type == MIRType::Int32) {
      div->setTruncateKind(Truncate);
    }
    return div;
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;
  void analyzeEdgeCasesForward() override;
  void analyzeEdgeCasesBackward() override;

  double getIdentity() override { MOZ_CRASH("not used"); }

  bool canBeNegativeZero() const { return canBeNegativeZero_; }
  void setCanBeNegativeZero(bool negativeZero) {
    canBeNegativeZero_ = negativeZero;
  }

  bool canBeNegativeOverflow() const { return canBeNegativeOverflow_; }

  bool canBeDivideByZero() const { return canBeDivideByZero_; }

  bool canBeNegativeDividend() const {
    // "Dividend" is an ambiguous concept for unsigned truncated
    // division, because of the truncation procedure:
    // ((x>>>0)/2)|0, for example, gets transformed in
    // MDiv::truncate into a node with lhs representing x (not
    // x>>>0) and rhs representing the constant 2; in other words,
    // the MIR node corresponds to "cast operands to unsigned and
    // divide" operation. In this case, is the dividend x or is it
    // x>>>0? In order to resolve such ambiguities, we disallow
    // the usage of this method for unsigned division.
    MOZ_ASSERT(!unsigned_);
    return canBeNegativeDividend_;
  }

  bool isUnsigned() const { return unsigned_; }

  bool isTruncatedIndirectly() const {
    return truncateKind() >= IndirectTruncate;
  }

  bool canTruncateInfinities() const { return isTruncated(); }
  bool canTruncateRemainder() const { return isTruncated(); }
  bool canTruncateOverflow() const {
    return isTruncated() || isTruncatedIndirectly();
  }
  bool canTruncateNegativeZero() const {
    return isTruncated() || isTruncatedIndirectly();
  }

  bool trapOnError() const { return trapOnError_; }
  wasm::BytecodeOffset bytecodeOffset() const {
    MOZ_ASSERT(bytecodeOffset_.isValid());
    return bytecodeOffset_;
  }

  bool isFloat32Commutative() const override { return true; }

  void computeRange(TempAllocator& alloc) override;
  bool fallible() const;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  void collectRangeInfoPreTrunc() override;
  TruncateKind operandTruncateKind(size_t index) const override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!MBinaryArithInstruction::congruentTo(ins)) {
      return false;
    }
    const MDiv* other = ins->toDiv();
    MOZ_ASSERT(other->trapOnError() == trapOnError_);
    return unsigned_ == other->isUnsigned();
  }

  ALLOW_CLONE(MDiv)
};

class MMod : public MBinaryArithInstruction {
  bool unsigned_;  // If false, signedness will be derived from operands
  bool canBeNegativeDividend_;
  bool canBePowerOfTwoDivisor_;
  bool canBeDivideByZero_;
  bool trapOnError_;
  wasm::BytecodeOffset bytecodeOffset_;

  MMod(MDefinition* left, MDefinition* right, MIRType type)
      : MBinaryArithInstruction(classOpcode, left, right, type),
        unsigned_(false),
        canBeNegativeDividend_(true),
        canBePowerOfTwoDivisor_(true),
        canBeDivideByZero_(true),
        trapOnError_(false) {}

 public:
  INSTRUCTION_HEADER(Mod)

  static MMod* New(TempAllocator& alloc, MDefinition* left, MDefinition* right,
                   MIRType type) {
    return new (alloc) MMod(left, right, type);
  }
  static MMod* New(
      TempAllocator& alloc, MDefinition* left, MDefinition* right, MIRType type,
      bool unsignd, bool trapOnError = false,
      wasm::BytecodeOffset bytecodeOffset = wasm::BytecodeOffset()) {
    auto* mod = new (alloc) MMod(left, right, type);
    mod->unsigned_ = unsignd;
    mod->trapOnError_ = trapOnError;
    mod->bytecodeOffset_ = bytecodeOffset;
    if (trapOnError) {
      mod->setGuard();  // not removable because of possible side-effects.
      mod->setNotMovable();
    }
    if (type == MIRType::Int32) {
      mod->setTruncateKind(Truncate);
    }
    return mod;
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  double getIdentity() override { MOZ_CRASH("not used"); }

  bool canBeNegativeDividend() const {
    MOZ_ASSERT(specialization_ == MIRType::Int32 ||
               specialization_ == MIRType::Int64);
    MOZ_ASSERT(!unsigned_);
    return canBeNegativeDividend_;
  }

  bool canBeDivideByZero() const {
    MOZ_ASSERT(specialization_ == MIRType::Int32 ||
               specialization_ == MIRType::Int64);
    return canBeDivideByZero_;
  }

  bool canBePowerOfTwoDivisor() const {
    MOZ_ASSERT(specialization_ == MIRType::Int32);
    return canBePowerOfTwoDivisor_;
  }

  void analyzeEdgeCasesForward() override;

  bool isUnsigned() const { return unsigned_; }

  bool trapOnError() const { return trapOnError_; }
  wasm::BytecodeOffset bytecodeOffset() const {
    MOZ_ASSERT(bytecodeOffset_.isValid());
    return bytecodeOffset_;
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  bool fallible() const;

  void computeRange(TempAllocator& alloc) override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;
  void collectRangeInfoPreTrunc() override;
  TruncateKind operandTruncateKind(size_t index) const override;

  bool congruentTo(const MDefinition* ins) const override {
    return MBinaryArithInstruction::congruentTo(ins) &&
           unsigned_ == ins->toMod()->isUnsigned();
  }

  bool possiblyCalls() const override { return type() == MIRType::Double; }

  ALLOW_CLONE(MMod)
};

class MConcat : public MBinaryInstruction,
                public MixPolicy<ConvertToStringPolicy<0>,
                                 ConvertToStringPolicy<1>>::Data {
  MConcat(MDefinition* left, MDefinition* right)
      : MBinaryInstruction(classOpcode, left, right) {
    // At least one input should be definitely string
    MOZ_ASSERT(left->type() == MIRType::String ||
               right->type() == MIRType::String);

    setMovable();
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(Concat)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MConcat)
};

class MCharCodeAt
    : public MBinaryInstruction,
      public MixPolicy<StringPolicy<0>, UnboxedInt32Policy<1>>::Data {
  MCharCodeAt(MDefinition* str, MDefinition* index)
      : MBinaryInstruction(classOpcode, str, index) {
    setMovable();
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(CharCodeAt)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  virtual AliasSet getAliasSet() const override {
    // Strings are immutable, so there is no implicit dependency.
    return AliasSet::None();
  }

  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MCharCodeAt)
};

class MFromCharCode : public MUnaryInstruction,
                      public UnboxedInt32Policy<0>::Data {
  explicit MFromCharCode(MDefinition* code)
      : MUnaryInstruction(classOpcode, code) {
    setMovable();
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(FromCharCode)
  TRIVIAL_NEW_WRAPPERS

  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MFromCharCode)
};

class MFromCodePoint : public MUnaryInstruction,
                       public UnboxedInt32Policy<0>::Data {
  explicit MFromCodePoint(MDefinition* codePoint)
      : MUnaryInstruction(classOpcode, codePoint) {
    setGuard();  // throws on invalid code point
    setMovable();
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(FromCodePoint)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  ALLOW_CLONE(MFromCodePoint)
};

class MStringConvertCase : public MUnaryInstruction,
                           public StringPolicy<0>::Data {
 public:
  enum Mode { LowerCase, UpperCase };

 private:
  Mode mode_;

  MStringConvertCase(MDefinition* string, Mode mode)
      : MUnaryInstruction(classOpcode, string), mode_(mode) {
    setResultType(MIRType::String);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(StringConvertCase)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, string))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           ins->toStringConvertCase()->mode() == mode();
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool possiblyCalls() const override { return true; }
  Mode mode() const { return mode_; }
};

class MStringSplit : public MBinaryInstruction,
                     public MixPolicy<StringPolicy<0>, StringPolicy<1>>::Data {
  CompilerObjectGroup group_;

  MStringSplit(TempAllocator& alloc, CompilerConstraintList* constraints,
               MDefinition* string, MDefinition* sep, ObjectGroup* group)
      : MBinaryInstruction(classOpcode, string, sep), group_(group) {
    setResultType(MIRType::Object);
    TemporaryTypeSet* types = MakeSingletonTypeSet(alloc, constraints, group);
    setResultTypeSet(types);
  }

 public:
  INSTRUCTION_HEADER(StringSplit)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC
  NAMED_OPERANDS((0, string), (1, separator))

  ObjectGroup* group() const { return group_; }
  bool possiblyCalls() const override { return true; }
  virtual AliasSet getAliasSet() const override {
    // Although this instruction returns a new array, we don't have to mark
    // it as store instruction, see also MNewArray.
    return AliasSet::None();
  }
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(group_);
  }
};

// Returns the value to use as |this| value. See also ComputeThis and
// BoxNonStrictThis in Interpreter.h.
class MComputeThis : public MUnaryInstruction, public BoxPolicy<0>::Data {
  explicit MComputeThis(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(ComputeThis)
  TRIVIAL_NEW_WRAPPERS

  bool possiblyCalls() const override { return true; }

  // Note: don't override getAliasSet: the thisValue hook can be effectful.
};

class MImplicitThis : public MUnaryInstruction,
                      public SingleObjectPolicy::Data {
  CompilerPropertyName name_;

  MImplicitThis(MDefinition* envChain, PropertyName* name)
      : MUnaryInstruction(classOpcode, envChain), name_(name) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(ImplicitThis)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, envChain))

  PropertyName* name() const { return name_; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(name_);
  }

  bool possiblyCalls() const override { return true; }
};

// Load an arrow function's |new.target| value.
class MArrowNewTarget : public MUnaryInstruction,
                        public SingleObjectPolicy::Data {
  explicit MArrowNewTarget(MDefinition* callee)
      : MUnaryInstruction(classOpcode, callee) {
    setResultType(MIRType::Value);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ArrowNewTarget)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, callee))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    // An arrow function's lexical |this| value is immutable.
    return AliasSet::None();
  }
};

// This is a 3 state flag used by FlagPhiInputsAsHavingRemovedUses to record and
// propagate the information about the consumers of a Phi instruction. This is
// then used to set UseRemoved flags on the inputs of such Phi instructions.
enum class PhiUsage : uint8_t { Unknown, Unused, Used };

using PhiVector = Vector<MPhi*, 4, JitAllocPolicy>;

class MPhi final : public MDefinition,
                   public InlineListNode<MPhi>,
                   public NoTypePolicy::Data {
  using InputVector = js::Vector<MUse, 2, JitAllocPolicy>;
  InputVector inputs_;

  TruncateKind truncateKind_;
  bool hasBackedgeType_;
  bool triedToSpecialize_;
  bool isIterator_;
  bool canProduceFloat32_;
  bool canConsumeFloat32_;
  // Record the state of the data flow before any mutation made to the control
  // flow, such that removing branches is properly accounted for.
  PhiUsage usageAnalysis_;

#if DEBUG
  bool specialized_;
#endif

 protected:
  MUse* getUseFor(size_t index) override {
    // Note: after the initial IonBuilder pass, it is OK to change phi
    // operands such that they do not include the type sets of their
    // operands. This can arise during e.g. value numbering, where
    // definitions producing the same value may have different type sets.
    MOZ_ASSERT(index < numOperands());
    return &inputs_[index];
  }
  const MUse* getUseFor(size_t index) const override { return &inputs_[index]; }

 public:
  INSTRUCTION_HEADER_WITHOUT_TYPEPOLICY(Phi)
  virtual const TypePolicy* typePolicy();
  virtual MIRType typePolicySpecialization();

  MPhi(TempAllocator& alloc, MIRType resultType)
      : MDefinition(classOpcode),
        inputs_(alloc),
        truncateKind_(NoTruncate),
        hasBackedgeType_(false),
        triedToSpecialize_(false),
        isIterator_(false),
        canProduceFloat32_(false),
        canConsumeFloat32_(false),
        usageAnalysis_(PhiUsage::Unknown)
#if DEBUG
        ,
        specialized_(false)
#endif
  {
    setResultType(resultType);
  }

  static MPhi* New(TempAllocator& alloc, MIRType resultType = MIRType::Value) {
    return new (alloc) MPhi(alloc, resultType);
  }
  static MPhi* New(TempAllocator::Fallible alloc,
                   MIRType resultType = MIRType::Value) {
    return new (alloc) MPhi(alloc.alloc, resultType);
  }

  void removeOperand(size_t index);
  void removeAllOperands();

  MDefinition* getOperand(size_t index) const override {
    return inputs_[index].producer();
  }
  size_t numOperands() const override { return inputs_.length(); }
  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u >= &inputs_[0]);
    MOZ_ASSERT(u <= &inputs_[numOperands() - 1]);
    return u - &inputs_[0];
  }
  void replaceOperand(size_t index, MDefinition* operand) final {
    inputs_[index].replaceProducer(operand);
  }
  bool hasBackedgeType() const { return hasBackedgeType_; }
  bool triedToSpecialize() const { return triedToSpecialize_; }
  void specialize(MIRType type) {
    triedToSpecialize_ = true;
    setResultType(type);
  }
  bool specializeType(TempAllocator& alloc);

#ifdef DEBUG
  // Assert that this is a phi in a loop header with a unique predecessor and
  // a unique backedge.
  void assertLoopPhi() const;
#else
  void assertLoopPhi() const {}
#endif

  // Assuming this phi is in a loop header with a unique loop entry, return
  // the phi operand along the loop entry.
  MDefinition* getLoopPredecessorOperand() const {
    assertLoopPhi();
    return getOperand(0);
  }

  // Assuming this phi is in a loop header with a unique loop entry, return
  // the phi operand along the loop backedge.
  MDefinition* getLoopBackedgeOperand() const {
    assertLoopPhi();
    return getOperand(1);
  }

  // Whether this phi's type already includes information for def.
  bool typeIncludes(MDefinition* def);

  // Add types for this phi which speculate about new inputs that may come in
  // via a loop backedge.
  MOZ_MUST_USE bool addBackedgeType(TempAllocator& alloc, MIRType type,
                                    TemporaryTypeSet* typeSet);

  // Mark all phis in |iterators|, and the phis they flow into, as having
  // implicit uses.
  static MOZ_MUST_USE bool markIteratorPhis(const PhiVector& iterators);

  // Initializes the operands vector to the given capacity,
  // permitting use of addInput() instead of addInputSlow().
  MOZ_MUST_USE bool reserveLength(size_t length) {
    return inputs_.reserve(length);
  }

  // Use only if capacity has been reserved by reserveLength
  void addInput(MDefinition* ins) { inputs_.infallibleEmplaceBack(ins, this); }

  // Appends a new input to the input vector. May perform reallocation.
  // Prefer reserveLength() and addInput() instead, where possible.
  MOZ_MUST_USE bool addInputSlow(MDefinition* ins) {
    return inputs_.emplaceBack(ins, this);
  }

  // Appends a new input to the input vector. Infallible because
  // we know the inputs fits in the vector's inline storage.
  void addInlineInput(MDefinition* ins) {
    MOZ_ASSERT(inputs_.length() < InputVector::InlineLength);
    MOZ_ALWAYS_TRUE(addInputSlow(ins));
  }

  // Update the type of this phi after adding |ins| as an input. Set
  // |*ptypeChange| to true if the type changed.
  bool checkForTypeChange(TempAllocator& alloc, MDefinition* ins,
                          bool* ptypeChange);

  MDefinition* foldsTo(TempAllocator& alloc) override;
  MDefinition* foldsTernary(TempAllocator& alloc);
  MDefinition* foldsFilterTypeSet();

  bool congruentTo(const MDefinition* ins) const override;
  bool updateForReplacement(MDefinition* def) override;

  bool isIterator() const { return isIterator_; }
  void setIterator() { isIterator_ = true; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;

  MDefinition* operandIfRedundant();

  bool canProduceFloat32() const override { return canProduceFloat32_; }

  void setCanProduceFloat32(bool can) { canProduceFloat32_ = can; }

  bool canConsumeFloat32(MUse* use) const override {
    return canConsumeFloat32_;
  }

  void setCanConsumeFloat32(bool can) { canConsumeFloat32_ = can; }

  TruncateKind operandTruncateKind(size_t index) const override;
  bool needTruncation(TruncateKind kind) override;
  void truncate() override;

  PhiUsage getUsageAnalysis() const { return usageAnalysis_; }
  void setUsageAnalysis(PhiUsage pu) {
    MOZ_ASSERT(usageAnalysis_ == PhiUsage::Unknown);
    usageAnalysis_ = pu;
    MOZ_ASSERT(usageAnalysis_ != PhiUsage::Unknown);
  }
};

// The goal of a Beta node is to split a def at a conditionally taken
// branch, so that uses dominated by it have a different name.
class MBeta : public MUnaryInstruction, public NoTypePolicy::Data {
 private:
  // This is the range induced by a comparison and branch in a preceding
  // block. Note that this does not reflect any range constraints from
  // the input value itself, so this value may differ from the range()
  // range after it is computed.
  const Range* comparison_;

  MBeta(MDefinition* val, const Range* comp)
      : MUnaryInstruction(classOpcode, val), comparison_(comp) {
    setResultType(val->type());
    setResultTypeSet(val->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(Beta)
  TRIVIAL_NEW_WRAPPERS

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  void computeRange(TempAllocator& alloc) override;
};

// If input evaluates to false (i.e. it's NaN, 0 or -0), 0 is returned, else the
// input is returned
class MNaNToZero : public MUnaryInstruction, public DoublePolicy<0>::Data {
  bool operandIsNeverNaN_;
  bool operandIsNeverNegativeZero_;

  explicit MNaNToZero(MDefinition* input)
      : MUnaryInstruction(classOpcode, input),
        operandIsNeverNaN_(false),
        operandIsNeverNegativeZero_(false) {
    setResultType(MIRType::Double);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(NaNToZero)
  TRIVIAL_NEW_WRAPPERS

  bool operandIsNeverNaN() const { return operandIsNeverNaN_; }

  bool operandIsNeverNegativeZero() const {
    return operandIsNeverNegativeZero_;
  }

  void collectRangeInfoPreTrunc() override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  void computeRange(TempAllocator& alloc) override;

  bool writeRecoverData(CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MNaNToZero)
};

// MIR representation of a Value on the OSR BaselineFrame.
// The Value is indexed off of OsrFrameReg.
class MOsrValue : public MUnaryInstruction, public NoTypePolicy::Data {
 private:
  ptrdiff_t frameOffset_;

  MOsrValue(MOsrEntry* entry, ptrdiff_t frameOffset)
      : MUnaryInstruction(classOpcode, entry), frameOffset_(frameOffset) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(OsrValue)
  TRIVIAL_NEW_WRAPPERS

  ptrdiff_t frameOffset() const { return frameOffset_; }

  MOsrEntry* entry() { return getOperand(0)->toOsrEntry(); }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// MIR representation of a JSObject scope chain pointer on the OSR
// BaselineFrame. The pointer is indexed off of OsrFrameReg.
class MOsrEnvironmentChain : public MUnaryInstruction,
                             public NoTypePolicy::Data {
 private:
  explicit MOsrEnvironmentChain(MOsrEntry* entry)
      : MUnaryInstruction(classOpcode, entry) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(OsrEnvironmentChain)
  TRIVIAL_NEW_WRAPPERS

  MOsrEntry* entry() { return getOperand(0)->toOsrEntry(); }
};

// MIR representation of a JSObject ArgumentsObject pointer on the OSR
// BaselineFrame. The pointer is indexed off of OsrFrameReg.
class MOsrArgumentsObject : public MUnaryInstruction,
                            public NoTypePolicy::Data {
 private:
  explicit MOsrArgumentsObject(MOsrEntry* entry)
      : MUnaryInstruction(classOpcode, entry) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(OsrArgumentsObject)
  TRIVIAL_NEW_WRAPPERS

  MOsrEntry* entry() { return getOperand(0)->toOsrEntry(); }
};

// MIR representation of the return value on the OSR BaselineFrame.
// The Value is indexed off of OsrFrameReg.
class MOsrReturnValue : public MUnaryInstruction, public NoTypePolicy::Data {
 private:
  explicit MOsrReturnValue(MOsrEntry* entry)
      : MUnaryInstruction(classOpcode, entry) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(OsrReturnValue)
  TRIVIAL_NEW_WRAPPERS

  MOsrEntry* entry() { return getOperand(0)->toOsrEntry(); }
};

class MBinaryCache : public MBinaryInstruction,
                     public MixPolicy<BoxPolicy<0>, BoxPolicy<1>>::Data {
 protected:
  explicit MBinaryCache(MDefinition* left, MDefinition* right, MIRType resType)
      : MBinaryInstruction(classOpcode, left, right) {
    setResultType(resType);
  }

 public:
  INSTRUCTION_HEADER(BinaryCache)
  TRIVIAL_NEW_WRAPPERS
};

class MUnaryCache : public MUnaryInstruction, public BoxPolicy<0>::Data {
  explicit MUnaryCache(MDefinition* input)
      : MUnaryInstruction(classOpcode, input) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(UnaryCache)
  TRIVIAL_NEW_WRAPPERS
};

// Check the current frame for over-recursion past the global stack limit.
class MCheckOverRecursed : public MNullaryInstruction {
  MCheckOverRecursed() : MNullaryInstruction(classOpcode) { setGuard(); }

 public:
  INSTRUCTION_HEADER(CheckOverRecursed)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Check whether we need to fire the interrupt handler.
class MInterruptCheck : public MNullaryInstruction {
  MInterruptCheck() : MNullaryInstruction(classOpcode) { setGuard(); }

 public:
  INSTRUCTION_HEADER(InterruptCheck)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Check whether we need to fire the interrupt handler (in wasm code).
class MWasmInterruptCheck : public MUnaryInstruction,
                            public NoTypePolicy::Data {
  wasm::BytecodeOffset bytecodeOffset_;

  MWasmInterruptCheck(MDefinition* tlsPointer,
                      wasm::BytecodeOffset bytecodeOffset)
      : MUnaryInstruction(classOpcode, tlsPointer),
        bytecodeOffset_(bytecodeOffset) {
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(WasmInterruptCheck)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, tlsPtr))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }
};

// Directly jumps to the indicated trap, leaving Wasm code and reporting a
// runtime error.

class MWasmTrap : public MAryControlInstruction<0, 0>,
                  public NoTypePolicy::Data {
  wasm::Trap trap_;
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmTrap(wasm::Trap trap, wasm::BytecodeOffset bytecodeOffset)
      : MAryControlInstruction(classOpcode),
        trap_(trap),
        bytecodeOffset_(bytecodeOffset) {}

 public:
  INSTRUCTION_HEADER(WasmTrap)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  wasm::Trap trap() const { return trap_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }
};

// Checks if a value is JS_UNINITIALIZED_LEXICAL, bailout out if so, leaving
// it to baseline to throw at the correct pc.
class MLexicalCheck : public MUnaryInstruction, public BoxPolicy<0>::Data {
  BailoutKind kind_;
  explicit MLexicalCheck(MDefinition* input,
                         BailoutKind kind = Bailout_UninitializedLexical)
      : MUnaryInstruction(classOpcode, input), kind_(kind) {
    setResultType(MIRType::Value);
    setResultTypeSet(input->resultTypeSet());
    setMovable();
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(LexicalCheck)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  BailoutKind bailoutKind() const { return kind_; }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
};

// Unconditionally throw an uninitialized let error.
class MThrowRuntimeLexicalError : public MNullaryInstruction {
  unsigned errorNumber_;

  explicit MThrowRuntimeLexicalError(unsigned errorNumber)
      : MNullaryInstruction(classOpcode), errorNumber_(errorNumber) {
    setGuard();
    setResultType(MIRType::None);
  }

 public:
  INSTRUCTION_HEADER(ThrowRuntimeLexicalError)
  TRIVIAL_NEW_WRAPPERS

  unsigned errorNumber() const { return errorNumber_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ExceptionState);
  }
};

// In the prologues of global and eval scripts, check for redeclarations.
class MGlobalNameConflictsCheck : public MNullaryInstruction {
  MGlobalNameConflictsCheck() : MNullaryInstruction(classOpcode) { setGuard(); }

 public:
  INSTRUCTION_HEADER(GlobalNameConflictsCheck)
  TRIVIAL_NEW_WRAPPERS
};

// If not defined, set a global variable to |undefined|.
class MDefVar : public MUnaryInstruction, public NoTypePolicy::Data {
 private:
  explicit MDefVar(MDefinition* envChain)
      : MUnaryInstruction(classOpcode, envChain) {}

 public:
  INSTRUCTION_HEADER(DefVar)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, environmentChain))

  bool possiblyCalls() const override { return true; }
};

class MDefLexical : public MUnaryInstruction, public NoTypePolicy::Data {
 private:
  explicit MDefLexical(MDefinition* envChain)
      : MUnaryInstruction(classOpcode, envChain) {}

 public:
  INSTRUCTION_HEADER(DefLexical)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, environmentChain))

  bool possiblyCalls() const override { return true; }
};

class MDefFun : public MBinaryInstruction, public ObjectPolicy<0>::Data {
 private:
  MDefFun(MDefinition* fun, MDefinition* envChain)
      : MBinaryInstruction(classOpcode, fun, envChain) {}

 public:
  INSTRUCTION_HEADER(DefFun)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, fun), (1, environmentChain))

  bool possiblyCalls() const override { return true; }
};

class MRegExp : public MNullaryInstruction {
  CompilerGCPointer<RegExpObject*> source_;
  bool hasShared_;

  MRegExp(TempAllocator& alloc, CompilerConstraintList* constraints,
          RegExpObject* source, bool hasShared)
      : MNullaryInstruction(classOpcode),
        source_(source),
        hasShared_(hasShared) {
    setResultType(MIRType::Object);
    if (!JitOptions.warpBuilder) {
      setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, source));
    }
  }

 public:
  INSTRUCTION_HEADER(RegExp)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC

  bool hasShared() const { return hasShared_; }
  RegExpObject* source() const { return source_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(source_);
  }
};

class MRegExpMatcher : public MTernaryInstruction,
                       public MixPolicy<ObjectPolicy<0>, StringPolicy<1>,
                                        UnboxedInt32Policy<2>>::Data {
 private:
  MRegExpMatcher(MDefinition* regexp, MDefinition* string,
                 MDefinition* lastIndex)
      : MTernaryInstruction(classOpcode, regexp, string, lastIndex) {
    setMovable();
    // May be object or null.
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(RegExpMatcher)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, regexp), (1, string), (2, lastIndex))

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;

  bool canRecoverOnBailout() const override { return true; }

  bool possiblyCalls() const override { return true; }
};

class MRegExpSearcher : public MTernaryInstruction,
                        public MixPolicy<ObjectPolicy<0>, StringPolicy<1>,
                                         UnboxedInt32Policy<2>>::Data {
 private:
  MRegExpSearcher(MDefinition* regexp, MDefinition* string,
                  MDefinition* lastIndex)
      : MTernaryInstruction(classOpcode, regexp, string, lastIndex) {
    setMovable();
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(RegExpSearcher)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, regexp), (1, string), (2, lastIndex))

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;

  bool canRecoverOnBailout() const override { return true; }

  bool possiblyCalls() const override { return true; }
};

class MRegExpTester : public MTernaryInstruction,
                      public MixPolicy<ObjectPolicy<0>, StringPolicy<1>,
                                       UnboxedInt32Policy<2>>::Data {
 private:
  MRegExpTester(MDefinition* regexp, MDefinition* string,
                MDefinition* lastIndex)
      : MTernaryInstruction(classOpcode, regexp, string, lastIndex) {
    setMovable();
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(RegExpTester)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, regexp), (1, string), (2, lastIndex))

  bool possiblyCalls() const override { return true; }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

class MRegExpPrototypeOptimizable : public MUnaryInstruction,
                                    public SingleObjectPolicy::Data {
  explicit MRegExpPrototypeOptimizable(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(RegExpPrototypeOptimizable)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MRegExpInstanceOptimizable
    : public MBinaryInstruction,
      public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>>::Data {
  explicit MRegExpInstanceOptimizable(MDefinition* object, MDefinition* proto)
      : MBinaryInstruction(classOpcode, object, proto) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(RegExpInstanceOptimizable)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, proto))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MGetFirstDollarIndex : public MUnaryInstruction,
                             public StringPolicy<0>::Data {
  explicit MGetFirstDollarIndex(MDefinition* str)
      : MUnaryInstruction(classOpcode, str) {
    setResultType(MIRType::Int32);

    // Codegen assumes string length > 0. Don't allow LICM to move this
    // before the .length > 1 check in RegExpReplace in RegExp.js.
    MOZ_ASSERT(!isMovable());
  }

 public:
  INSTRUCTION_HEADER(GetFirstDollarIndex)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, str))

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MDefinition* foldsTo(TempAllocator& alloc) override;
};

class MStringReplace : public MTernaryInstruction,
                       public MixPolicy<StringPolicy<0>, StringPolicy<1>,
                                        StringPolicy<2>>::Data {
 private:
  bool isFlatReplacement_;

  MStringReplace(MDefinition* string, MDefinition* pattern,
                 MDefinition* replacement)
      : MTernaryInstruction(classOpcode, string, pattern, replacement),
        isFlatReplacement_(false) {
    setMovable();
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(StringReplace)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, string), (1, pattern), (2, replacement))

  void setFlatReplacement() {
    MOZ_ASSERT(!isFlatReplacement_);
    isFlatReplacement_ = true;
  }

  bool isFlatReplacement() const { return isFlatReplacement_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isStringReplace()) {
      return false;
    }
    if (isFlatReplacement_ != ins->toStringReplace()->isFlatReplacement()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override {
    if (isFlatReplacement_) {
      MOZ_ASSERT(!pattern()->isRegExp());
      return true;
    }
    return false;
  }

  bool possiblyCalls() const override { return true; }
};

class MSubstr : public MTernaryInstruction,
                public MixPolicy<StringPolicy<0>, UnboxedInt32Policy<1>,
                                 UnboxedInt32Policy<2>>::Data {
 private:
  MSubstr(MDefinition* string, MDefinition* begin, MDefinition* length)
      : MTernaryInstruction(classOpcode, string, begin, length) {
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(Substr)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, string), (1, begin), (2, length))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MClassConstructor : public MNullaryInstruction {
  jsbytecode* pc_;

  explicit MClassConstructor(jsbytecode* pc)
      : MNullaryInstruction(classOpcode), pc_(pc) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(ClassConstructor)
  TRIVIAL_NEW_WRAPPERS

  jsbytecode* pc() const { return pc_; }

  bool possiblyCalls() const override { return true; }
};

class MDerivedClassConstructor : public MUnaryInstruction,
                                 public SingleObjectPolicy::Data {
  jsbytecode* pc_;

  MDerivedClassConstructor(MDefinition* prototype, jsbytecode* pc)
      : MUnaryInstruction(classOpcode, prototype), pc_(pc) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(DerivedClassConstructor)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, prototype))

  jsbytecode* pc() const { return pc_; }

  bool possiblyCalls() const override { return true; }
};

class MModuleMetadata : public MNullaryInstruction {
  CompilerObject module_;

  explicit MModuleMetadata(JSObject* module)
      : MNullaryInstruction(classOpcode), module_(module) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(ModuleMetadata)
  TRIVIAL_NEW_WRAPPERS

  JSObject* module() const { return module_; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(module_);
  }
};

class MDynamicImport : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MDynamicImport(MDefinition* specifier)
      : MUnaryInstruction(classOpcode, specifier) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(DynamicImport)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, specifier))
};

struct LambdaFunctionInfo {
  // The functions used in lambdas are the canonical original function in
  // the script, and are immutable except for delazification. Record this
  // information while still on the main thread to avoid races.
 private:
  CompilerFunction fun_;

 public:
  js::BaseScript* baseScript;
  js::FunctionFlags flags;
  uint16_t nargs;
  bool singletonType;
  bool useSingletonForClone;

  LambdaFunctionInfo(JSFunction* fun, BaseScript* baseScript,
                     FunctionFlags flags, uint16_t nargs, bool singletonType,
                     bool useSingletonForClone)
      : fun_(fun),
        baseScript(baseScript),
        flags(flags),
        nargs(nargs),
        singletonType(singletonType),
        useSingletonForClone(useSingletonForClone) {}

  LambdaFunctionInfo(const LambdaFunctionInfo& other)
      : fun_(static_cast<JSFunction*>(other.fun_)),
        baseScript(other.baseScript),
        flags(other.flags),
        nargs(other.nargs),
        singletonType(other.singletonType),
        useSingletonForClone(other.useSingletonForClone) {}

  // Be careful when calling this off-thread. Don't call any JSFunction*
  // methods that depend on script/lazyScript - this can race with
  // delazification on the main thread.
  JSFunction* funUnsafe() const { return fun_; }

  bool appendRoots(MRootList& roots) const {
    if (!roots.append(fun_)) {
      return false;
    }
    return roots.append(baseScript);
  }

 private:
  void operator=(const LambdaFunctionInfo&) = delete;
};

class MLambda : public MBinaryInstruction, public SingleObjectPolicy::Data {
  const LambdaFunctionInfo info_;

  MLambda(TempAllocator& alloc, CompilerConstraintList* constraints,
          MDefinition* envChain, MConstant* cst, const LambdaFunctionInfo& info)
      : MBinaryInstruction(classOpcode, envChain, cst), info_(info) {
    setResultType(MIRType::Object);
    if (!JitOptions.warpBuilder) {
      JSFunction* fun = info.funUnsafe();
      if (!info.singletonType && !info.useSingletonForClone) {
        setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, fun));
      }
    }
  }

 public:
  INSTRUCTION_HEADER(Lambda)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC
  NAMED_OPERANDS((0, environmentChain))

  MConstant* functionOperand() const { return getOperand(1)->toConstant(); }
  const LambdaFunctionInfo& info() const { return info_; }
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return info_.appendRoots(roots);
  }
};

class MLambdaArrow
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>, ObjectPolicy<2>>::Data {
  const LambdaFunctionInfo info_;

  MLambdaArrow(TempAllocator& alloc, CompilerConstraintList* constraints,
               MDefinition* envChain, MDefinition* newTarget, MConstant* cst,
               const LambdaFunctionInfo& info)
      : MTernaryInstruction(classOpcode, envChain, newTarget, cst),
        info_(info) {
    setResultType(MIRType::Object);
    if (!JitOptions.warpBuilder) {
      JSFunction* fun = info.funUnsafe();
      MOZ_ASSERT(!info.useSingletonForClone);
      if (!info.singletonType) {
        setResultTypeSet(MakeSingletonTypeSet(alloc, constraints, fun));
      }
    }
  }

 public:
  INSTRUCTION_HEADER(LambdaArrow)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC
  NAMED_OPERANDS((0, environmentChain), (1, newTargetDef))

  MConstant* functionOperand() const { return getOperand(2)->toConstant(); }
  const LambdaFunctionInfo& info() const { return info_; }
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return info_.appendRoots(roots);
  }
};

class MFunctionWithProto : public MTernaryInstruction,
                           public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>,
                                            ObjectPolicy<2>>::Data {
  CompilerFunction fun_;

  MFunctionWithProto(MDefinition* envChain, MDefinition* prototype,
                     MConstant* cst)
      : MTernaryInstruction(classOpcode, envChain, prototype, cst),
        fun_(&cst->toObject().as<JSFunction>()) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(FunctionWithProto)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, environmentChain), (1, prototype))

  MConstant* functionOperand() const { return getOperand(2)->toConstant(); }
  JSFunction* function() const { return fun_; }
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(fun_);
  }

  bool possiblyCalls() const override { return true; }
};

class MSetFunName : public MBinaryInstruction,
                    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>>::Data {
  uint8_t prefixKind_;

  explicit MSetFunName(MDefinition* fun, MDefinition* name, uint8_t prefixKind)
      : MBinaryInstruction(classOpcode, fun, name), prefixKind_(prefixKind) {
    setResultType(MIRType::None);
  }

 public:
  INSTRUCTION_HEADER(SetFunName)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, fun), (1, name))

  uint8_t prefixKind() const { return prefixKind_; }

  bool possiblyCalls() const override { return true; }
};

// Returns obj->slots.
class MSlots : public MUnaryInstruction, public SingleObjectPolicy::Data {
  explicit MSlots(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::Slots);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Slots)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }

  ALLOW_CLONE(MSlots)
};

// Returns obj->elements.
class MElements : public MUnaryInstruction, public SingleObjectPolicy::Data {
  explicit MElements(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::Elements);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Elements)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }

  ALLOW_CLONE(MElements)
};

// A constant value for some object's typed array elements.
class MConstantElements : public MNullaryInstruction {
  SharedMem<void*> value_;

 protected:
  explicit MConstantElements(SharedMem<void*> v)
      : MNullaryInstruction(classOpcode), value_(v) {
    setResultType(MIRType::Elements);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ConstantElements)
  TRIVIAL_NEW_WRAPPERS

  SharedMem<void*> value() const { return value_; }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  HashNumber valueHash() const override {
    return (HashNumber)(size_t)value_.asValue();
  }

  bool congruentTo(const MDefinition* ins) const override {
    return ins->isConstantElements() &&
           ins->toConstantElements()->value() == value();
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MConstantElements)
};

// Passes through an object's elements, after ensuring it is entirely doubles.
class MConvertElementsToDoubles : public MUnaryInstruction,
                                  public NoTypePolicy::Data {
  explicit MConvertElementsToDoubles(MDefinition* elements)
      : MUnaryInstruction(classOpcode, elements) {
    setGuard();
    setMovable();
    setResultType(MIRType::Elements);
  }

 public:
  INSTRUCTION_HEADER(ConvertElementsToDoubles)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    // This instruction can read and write to the elements' contents.
    // However, it is alright to hoist this from loops which explicitly
    // read or write to the elements: such reads and writes will use double
    // values and can be reordered freely wrt this conversion, except that
    // definite double loads must follow the conversion. The latter
    // property is ensured by chaining this instruction with the elements
    // themselves, in the same manner as MBoundsCheck.
    return AliasSet::None();
  }
};

// If |elements| has the CONVERT_DOUBLE_ELEMENTS flag, convert value to
// double. Else return the original value.
class MMaybeToDoubleElement : public MBinaryInstruction,
                              public UnboxedInt32Policy<1>::Data {
  MMaybeToDoubleElement(MDefinition* elements, MDefinition* value)
      : MBinaryInstruction(classOpcode, elements, value) {
    MOZ_ASSERT(elements->type() == MIRType::Elements);
    setMovable();
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(MaybeToDoubleElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, value))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
};

// Passes through an object, after ensuring its elements are not copy on write.
class MMaybeCopyElementsForWrite : public MUnaryInstruction,
                                   public SingleObjectPolicy::Data {
  bool checkNative_;

  explicit MMaybeCopyElementsForWrite(MDefinition* object, bool checkNative)
      : MUnaryInstruction(classOpcode, object), checkNative_(checkNative) {
    setGuard();
    setMovable();
    setResultType(MIRType::Object);
    setResultTypeSet(object->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(MaybeCopyElementsForWrite)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool checkNative() const { return checkNative_; }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           checkNative() == ins->toMaybeCopyElementsForWrite()->checkNative();
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ObjectFields);
  }
#ifdef DEBUG
  bool needsResumePoint() const override {
    // This instruction is idempotent and does not change observable
    // behavior, so does not need its own resume point.
    return false;
  }
#endif
};

// Load the initialized length from an elements header.
class MInitializedLength : public MUnaryInstruction, public NoTypePolicy::Data {
  explicit MInitializedLength(MDefinition* elements)
      : MUnaryInstruction(classOpcode, elements) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(InitializedLength)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }

  void computeRange(TempAllocator& alloc) override;

  ALLOW_CLONE(MInitializedLength)
};

// Store to the initialized length in an elements header. Note the input is an
// *index*, one less than the desired length.
class MSetInitializedLength : public MBinaryInstruction,
                              public NoTypePolicy::Data {
  MSetInitializedLength(MDefinition* elements, MDefinition* index)
      : MBinaryInstruction(classOpcode, elements, index) {}

 public:
  INSTRUCTION_HEADER(SetInitializedLength)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index))

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ObjectFields);
  }

  ALLOW_CLONE(MSetInitializedLength)
};

// Load the array length from an elements header.
class MArrayLength : public MUnaryInstruction, public NoTypePolicy::Data {
  explicit MArrayLength(MDefinition* elements)
      : MUnaryInstruction(classOpcode, elements) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ArrayLength)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }

  void computeRange(TempAllocator& alloc) override;

  ALLOW_CLONE(MArrayLength)
};

// Store to the length in an elements header. Note the input is an *index*, one
// less than the desired length.
class MSetArrayLength : public MBinaryInstruction, public NoTypePolicy::Data {
  MSetArrayLength(MDefinition* elements, MDefinition* index)
      : MBinaryInstruction(classOpcode, elements, index) {}

 public:
  INSTRUCTION_HEADER(SetArrayLength)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index))

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ObjectFields);
  }

  // By default no, unless built as a recovered instruction.
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return isRecoveredOnBailout(); }
};

class MGetNextEntryForIterator
    : public MBinaryInstruction,
      public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>>::Data {
 public:
  enum Mode { Map, Set };

 private:
  Mode mode_;

  explicit MGetNextEntryForIterator(MDefinition* iter, MDefinition* result,
                                    Mode mode)
      : MBinaryInstruction(classOpcode, iter, result), mode_(mode) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(GetNextEntryForIterator)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, iter), (1, result))

  Mode mode() const { return mode_; }
};

// Read the length of a typed array.
class MTypedArrayLength : public MUnaryInstruction,
                          public SingleObjectPolicy::Data {
  explicit MTypedArrayLength(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(TypedArrayLength)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::TypedArrayLengthOrOffset);
  }

  void computeRange(TempAllocator& alloc) override;
};

// Read the byteOffset of a typed array.
class MTypedArrayByteOffset : public MUnaryInstruction,
                              public SingleObjectPolicy::Data {
  explicit MTypedArrayByteOffset(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(TypedArrayByteOffset)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::TypedArrayLengthOrOffset);
  }

  void computeRange(TempAllocator& alloc) override;
};

// Load a typed array's elements vector.
class MTypedArrayElements : public MUnaryInstruction,
                            public SingleObjectPolicy::Data {
  explicit MTypedArrayElements(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::Elements);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(TypedArrayElements)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }

  ALLOW_CLONE(MTypedArrayElements)
};

// Return the element shift of a typed array, i.e. the shift value so that
// |1 << shift| is equal to the element size.
class MTypedArrayElementShift : public MUnaryInstruction,
                                public SingleObjectPolicy::Data {
  explicit MTypedArrayElementShift(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(TypedArrayElementShift)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  void computeRange(TempAllocator& alloc) override;
};

// Convert the input into an Int32 value for accessing a TypedArray element.
// If the input is non-finite, not an integer, negative, or outside the Int32
// range, produces a value which is known to trigger an out-of-bounds access.
class MTypedArrayIndexToInt32 : public MUnaryInstruction,
                                public TypedArrayIndexPolicy::Data {
  explicit MTypedArrayIndexToInt32(MDefinition* def)
      : MUnaryInstruction(classOpcode, def) {
    MOZ_ASSERT(def->type() == MIRType::Int32 || def->type() == MIRType::Double);
    setResultType(MIRType::Int32);
    setMovable();
    specialization_ = def->type();
  }

 public:
  INSTRUCTION_HEADER(TypedArrayIndexToInt32)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MTypedArrayIndexToInt32)
};

class MKeepAliveObject : public MUnaryInstruction,
                         public SingleObjectPolicy::Data {
  explicit MKeepAliveObject(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::None);
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(KeepAliveObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))
};

// Perform !-operation
class MNot : public MUnaryInstruction, public TestPolicy::Data {
  bool operandMightEmulateUndefined_;
  bool operandIsNeverNaN_;

  explicit MNot(MDefinition* input,
                CompilerConstraintList* constraints = nullptr)
      : MUnaryInstruction(classOpcode, input),
        operandMightEmulateUndefined_(true),
        operandIsNeverNaN_(false) {
    setResultType(MIRType::Boolean);
    setMovable();
    if (constraints) {
      cacheOperandMightEmulateUndefined(constraints);
    }
  }

  void cacheOperandMightEmulateUndefined(CompilerConstraintList* constraints);

 public:
  static MNot* NewInt32(TempAllocator& alloc, MDefinition* input) {
    MOZ_ASSERT(input->type() == MIRType::Int32 ||
               input->type() == MIRType::Int64);
    auto* ins = new (alloc) MNot(input);
    ins->setResultType(MIRType::Int32);
    return ins;
  }

  INSTRUCTION_HEADER(Not)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  void markNoOperandEmulatesUndefined() {
    operandMightEmulateUndefined_ = false;
  }
  bool operandMightEmulateUndefined() const {
    return operandMightEmulateUndefined_;
  }
  bool operandIsNeverNaN() const { return operandIsNeverNaN_; }

  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }
  void collectRangeInfoPreTrunc() override;

  void trySpecializeFloat32(TempAllocator& alloc) override;
  bool isFloat32Commutative() const override { return true; }
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

// Bailout if index + minimum < 0 or index + maximum >= length. The length used
// in a bounds check must not be negative, or the wrong result may be computed
// (unsigned comparisons may be used).
class MBoundsCheck
    : public MBinaryInstruction,
      public MixPolicy<UnboxedInt32Policy<0>, UnboxedInt32Policy<1>>::Data {
  // Range over which to perform the bounds check, may be modified by GVN.
  int32_t minimum_;
  int32_t maximum_;
  bool fallible_;

  MBoundsCheck(MDefinition* index, MDefinition* length)
      : MBinaryInstruction(classOpcode, index, length),
        minimum_(0),
        maximum_(0),
        fallible_(true) {
    setGuard();
    setMovable();
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(length->type() == MIRType::Int32);

    // Returns the checked index.
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(BoundsCheck)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, index), (1, length))

  int32_t minimum() const { return minimum_; }
  void setMinimum(int32_t n) {
    MOZ_ASSERT(fallible_);
    minimum_ = n;
  }
  int32_t maximum() const { return maximum_; }
  void setMaximum(int32_t n) {
    MOZ_ASSERT(fallible_);
    maximum_ = n;
  }
  MDefinition* foldsTo(TempAllocator& alloc) override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isBoundsCheck()) {
      return false;
    }
    const MBoundsCheck* other = ins->toBoundsCheck();
    if (minimum() != other->minimum() || maximum() != other->maximum()) {
      return false;
    }
    if (fallible() != other->fallible()) {
      return false;
    }
    return congruentIfOperandsEqual(other);
  }
  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;
  bool fallible() const { return fallible_; }
  void collectRangeInfoPreTrunc() override;

  ALLOW_CLONE(MBoundsCheck)
};

// Bailout if index < minimum.
class MBoundsCheckLower : public MUnaryInstruction,
                          public UnboxedInt32Policy<0>::Data {
  int32_t minimum_;
  bool fallible_;

  explicit MBoundsCheckLower(MDefinition* index)
      : MUnaryInstruction(classOpcode, index), minimum_(0), fallible_(true) {
    setGuard();
    setMovable();
    MOZ_ASSERT(index->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(BoundsCheckLower)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, index))

  int32_t minimum() const { return minimum_; }
  void setMinimum(int32_t n) { minimum_ = n; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool fallible() const { return fallible_; }
  void collectRangeInfoPreTrunc() override;
};

class MSpectreMaskIndex
    : public MBinaryInstruction,
      public MixPolicy<UnboxedInt32Policy<0>, UnboxedInt32Policy<1>>::Data {
  MSpectreMaskIndex(MDefinition* index, MDefinition* length)
      : MBinaryInstruction(classOpcode, index, length) {
    // Note: this instruction does not need setGuard(): if there are no uses
    // it's fine for DCE to eliminate this instruction.
    setMovable();
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(length->type() == MIRType::Int32);

    // Returns the masked index.
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(SpectreMaskIndex)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, index), (1, length))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  virtual AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;

  ALLOW_CLONE(MSpectreMaskIndex)
};

// Instructions which access an object's elements can either do so on a
// definition accessing that elements pointer, or on the object itself, if its
// elements are inline. In the latter case there must be an offset associated
// with the access.
static inline bool IsValidElementsType(MDefinition* elements,
                                       int32_t offsetAdjustment) {
  return elements->type() == MIRType::Elements ||
         (elements->type() == MIRType::Object && offsetAdjustment != 0);
}

// Load a value from a dense array's element vector and does a hole check if the
// array is not known to be packed.
class MLoadElement : public MBinaryInstruction,
                     public SingleObjectPolicy::Data {
  bool needsHoleCheck_;
  bool loadDoubles_;
  int32_t offsetAdjustment_;

  MLoadElement(MDefinition* elements, MDefinition* index, bool needsHoleCheck,
               bool loadDoubles, int32_t offsetAdjustment = 0)
      : MBinaryInstruction(classOpcode, elements, index),
        needsHoleCheck_(needsHoleCheck),
        loadDoubles_(loadDoubles),
        offsetAdjustment_(offsetAdjustment) {
    if (needsHoleCheck) {
      // Uses may be optimized away based on this instruction's result
      // type. This means it's invalid to DCE this instruction, as we
      // have to invalidate when we read a hole.
      setGuard();
    }
    setResultType(MIRType::Value);
    setMovable();
    MOZ_ASSERT(IsValidElementsType(elements, offsetAdjustment));
    MOZ_ASSERT(index->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(LoadElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index))

  bool needsHoleCheck() const { return needsHoleCheck_; }
  bool loadDoubles() const { return loadDoubles_; }
  int32_t offsetAdjustment() const { return offsetAdjustment_; }
  bool fallible() const { return needsHoleCheck(); }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isLoadElement()) {
      return false;
    }
    const MLoadElement* other = ins->toLoadElement();
    if (needsHoleCheck() != other->needsHoleCheck()) {
      return false;
    }
    if (loadDoubles() != other->loadDoubles()) {
      return false;
    }
    if (offsetAdjustment() != other->offsetAdjustment()) {
      return false;
    }
    return congruentIfOperandsEqual(other);
  }
  AliasType mightAlias(const MDefinition* store) const override;
  MDefinition* foldsTo(TempAllocator& alloc) override;
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::Element);
  }

  ALLOW_CLONE(MLoadElement)
};

// Load a value from the elements vector of a native object. If the index is
// out-of-bounds, or the indexed slot has a hole, undefined is returned instead.
class MLoadElementHole : public MTernaryInstruction,
                         public SingleObjectPolicy::Data {
  bool needsNegativeIntCheck_;
  bool needsHoleCheck_;

  MLoadElementHole(MDefinition* elements, MDefinition* index,
                   MDefinition* initLength, bool needsHoleCheck)
      : MTernaryInstruction(classOpcode, elements, index, initLength),
        needsNegativeIntCheck_(true),
        needsHoleCheck_(needsHoleCheck) {
    setResultType(MIRType::Value);
    setMovable();

    // Set the guard flag to make sure we bail when we see a negative
    // index. We can clear this flag (and needsNegativeIntCheck_) in
    // collectRangeInfoPreTrunc.
    setGuard();

    MOZ_ASSERT(elements->type() == MIRType::Elements);
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(initLength->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(LoadElementHole)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, initLength))

  bool needsNegativeIntCheck() const { return needsNegativeIntCheck_; }
  bool needsHoleCheck() const { return needsHoleCheck_; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isLoadElementHole()) {
      return false;
    }
    const MLoadElementHole* other = ins->toLoadElementHole();
    if (needsHoleCheck() != other->needsHoleCheck()) {
      return false;
    }
    if (needsNegativeIntCheck() != other->needsNegativeIntCheck()) {
      return false;
    }
    return congruentIfOperandsEqual(other);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::Element);
  }
  void collectRangeInfoPreTrunc() override;

  ALLOW_CLONE(MLoadElementHole)
};

class MStoreElementCommon {
  MIRType elementType_;
  bool needsBarrier_;

 protected:
  MStoreElementCommon() : elementType_(MIRType::Value), needsBarrier_(false) {}

 public:
  MIRType elementType() const { return elementType_; }
  void setElementType(MIRType elementType) {
    MOZ_ASSERT(elementType != MIRType::None);
    elementType_ = elementType;
  }
  bool needsBarrier() const { return needsBarrier_; }
  void setNeedsBarrier() { needsBarrier_ = true; }
};

// This instruction is used to load an element of a non-escaped inlined array.
class MLoadElementFromState : public MBinaryInstruction,
                              public SingleObjectPolicy::Data {
  MLoadElementFromState(MDefinition* array, MDefinition* index)
      : MBinaryInstruction(classOpcode, array, index) {
    MOZ_ASSERT(array->isArgumentState());
    MOZ_ASSERT(index->type() == MIRType::Int32);
    setResultType(MIRType::Value);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(LoadElementFromState)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, array), (1, index));

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Store a value to a dense array slots vector.
class MStoreElement
    : public MTernaryInstruction,
      public MStoreElementCommon,
      public MixPolicy<SingleObjectPolicy, NoFloatPolicy<2>>::Data {
  bool needsHoleCheck_;
  int32_t offsetAdjustment_;

  MStoreElement(MDefinition* elements, MDefinition* index, MDefinition* value,
                bool needsHoleCheck, int32_t offsetAdjustment = 0)
      : MTernaryInstruction(classOpcode, elements, index, value) {
    needsHoleCheck_ = needsHoleCheck;
    offsetAdjustment_ = offsetAdjustment;
    MOZ_ASSERT(IsValidElementsType(elements, offsetAdjustment));
    MOZ_ASSERT(index->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(StoreElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, value))

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::Element);
  }
  bool needsHoleCheck() const { return needsHoleCheck_; }
  int32_t offsetAdjustment() const { return offsetAdjustment_; }
  bool fallible() const { return needsHoleCheck(); }

  ALLOW_CLONE(MStoreElement)
};

// Like MStoreElement, but supports indexes >= initialized length. The downside
// is that we cannot hoist the elements vector and bounds check, since this
// instruction may update the (initialized) length and reallocate the elements
// vector.
class MStoreElementHole
    : public MQuaternaryInstruction,
      public MStoreElementCommon,
      public MixPolicy<SingleObjectPolicy, NoFloatPolicy<3>>::Data {
  MStoreElementHole(MDefinition* object, MDefinition* elements,
                    MDefinition* index, MDefinition* value)
      : MQuaternaryInstruction(classOpcode, object, elements, index, value) {
    MOZ_ASSERT(elements->type() == MIRType::Elements);
    MOZ_ASSERT(index->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(StoreElementHole)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, elements), (2, index), (3, value))

  ALLOW_CLONE(MStoreElementHole)
};

// Try to store a value to a dense array slots vector. May fail due to the
// object being non-extensible/sealed/frozen. Cannot be used on an object that
// has extra indexed properties.
class MFallibleStoreElement
    : public MQuaternaryInstruction,
      public MStoreElementCommon,
      public MixPolicy<SingleObjectPolicy, NoFloatPolicy<3>>::Data {
  bool needsHoleCheck_;

  MFallibleStoreElement(MDefinition* object, MDefinition* elements,
                        MDefinition* index, MDefinition* value,
                        bool needsHoleCheck)
      : MQuaternaryInstruction(classOpcode, object, elements, index, value),
        needsHoleCheck_(needsHoleCheck) {
    MOZ_ASSERT(elements->type() == MIRType::Elements);
    MOZ_ASSERT(index->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(FallibleStoreElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, elements), (2, index), (3, value))

  bool needsHoleCheck() const { return needsHoleCheck_; }

  ALLOW_CLONE(MFallibleStoreElement)
};

// Array.prototype.pop or Array.prototype.shift on a dense array.
class MArrayPopShift : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
 public:
  enum Mode { Pop, Shift };

 private:
  Mode mode_;
  bool needsHoleCheck_;
  bool maybeUndefined_;

  MArrayPopShift(MDefinition* object, Mode mode, bool needsHoleCheck,
                 bool maybeUndefined)
      : MUnaryInstruction(classOpcode, object),
        mode_(mode),
        needsHoleCheck_(needsHoleCheck),
        maybeUndefined_(maybeUndefined) {}

 public:
  INSTRUCTION_HEADER(ArrayPopShift)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool needsHoleCheck() const { return needsHoleCheck_; }
  bool maybeUndefined() const { return maybeUndefined_; }
  bool mode() const { return mode_; }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ObjectFields | AliasSet::Element);
  }

  ALLOW_CLONE(MArrayPopShift)
};

// Array.prototype.push on a dense array. Returns the new array length.
class MArrayPush : public MBinaryInstruction,
                   public MixPolicy<SingleObjectPolicy, BoxPolicy<1>>::Data {
  MArrayPush(MDefinition* object, MDefinition* value)
      : MBinaryInstruction(classOpcode, object, value) {
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(ArrayPush)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, value))

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ObjectFields | AliasSet::Element);
  }
  void computeRange(TempAllocator& alloc) override;

  ALLOW_CLONE(MArrayPush)
};

// Array.prototype.slice on a dense array.
class MArraySlice : public MTernaryInstruction,
                    public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<1>,
                                     UnboxedInt32Policy<2>>::Data {
  CompilerObject templateObj_;
  gc::InitialHeap initialHeap_;

  MArraySlice(MDefinition* obj, MDefinition* begin, MDefinition* end,
              JSObject* templateObj, gc::InitialHeap initialHeap)
      : MTernaryInstruction(classOpcode, obj, begin, end),
        templateObj_(templateObj),
        initialHeap_(initialHeap) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(ArraySlice)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, begin), (2, end))

  JSObject* templateObj() const { return templateObj_; }

  gc::InitialHeap initialHeap() const { return initialHeap_; }

  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObj_);
  }
};

class MArrayJoin : public MBinaryInstruction,
                   public MixPolicy<ObjectPolicy<0>, StringPolicy<1>>::Data {
  bool optimizeForArray_;

  MArrayJoin(MDefinition* array, MDefinition* sep, bool optimizeForArray)
      : MBinaryInstruction(classOpcode, array, sep),
        optimizeForArray_(optimizeForArray) {
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(ArrayJoin)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, array), (1, sep))

  // MArrayJoin doesn't override |getAliasSet()|, because Array.prototype.join
  // might coerce the elements of the Array to strings. This coercion might
  // cause the evaluation of JavaScript code.

  bool optimizeForArray() const { return optimizeForArray_; }
  bool possiblyCalls() const override { return true; }
  MDefinition* foldsTo(TempAllocator& alloc) override;
};

// All barriered operations - MCompareExchangeTypedArrayElement,
// MExchangeTypedArrayElement, and MAtomicTypedArrayElementBinop, as
// well as MLoadUnboxedScalar and MStoreUnboxedScalar when they are
// marked as requiring a memory barrer - have the following
// attributes:
//
// - Not movable
// - Not removable
// - Not congruent with any other instruction
// - Effectful (they alias every TypedArray store)
//
// The intended effect of those constraints is to prevent all loads
// and stores preceding the barriered operation from being moved to
// after the barriered operation, and vice versa, and to prevent the
// barriered operation from being removed or hoisted.

enum MemoryBarrierRequirement {
  DoesNotRequireMemoryBarrier,
  DoesRequireMemoryBarrier
};

// Also see comments at MMemoryBarrierRequirement, above.

// Load an unboxed scalar value from a typed array or other object.
class MLoadUnboxedScalar : public MBinaryInstruction,
                           public SingleObjectPolicy::Data {
  Scalar::Type storageType_;
  Scalar::Type readType_;
  bool requiresBarrier_;
  int32_t offsetAdjustment_;
  bool canonicalizeDoubles_;

  MLoadUnboxedScalar(
      MDefinition* elements, MDefinition* index, Scalar::Type storageType,
      MemoryBarrierRequirement requiresBarrier = DoesNotRequireMemoryBarrier,
      int32_t offsetAdjustment = 0, bool canonicalizeDoubles = true)
      : MBinaryInstruction(classOpcode, elements, index),
        storageType_(storageType),
        readType_(storageType),
        requiresBarrier_(requiresBarrier == DoesRequireMemoryBarrier),
        offsetAdjustment_(offsetAdjustment),
        canonicalizeDoubles_(canonicalizeDoubles) {
    setResultType(MIRType::Value);
    if (requiresBarrier_) {
      setGuard();  // Not removable or movable
    } else {
      setMovable();
    }
    MOZ_ASSERT(IsValidElementsType(elements, offsetAdjustment));
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(storageType >= 0 && storageType < Scalar::MaxTypedArrayViewType);
  }

 public:
  INSTRUCTION_HEADER(LoadUnboxedScalar)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index))

  Scalar::Type readType() const { return readType_; }

  Scalar::Type storageType() const { return storageType_; }
  bool fallible() const {
    // Bailout if the result does not fit in an int32.
    return readType_ == Scalar::Uint32 && type() == MIRType::Int32;
  }
  bool requiresMemoryBarrier() const { return requiresBarrier_; }
  bool canonicalizeDoubles() const { return canonicalizeDoubles_; }
  int32_t offsetAdjustment() const { return offsetAdjustment_; }
  void setOffsetAdjustment(int32_t offsetAdjustment) {
    offsetAdjustment_ = offsetAdjustment;
  }
  AliasSet getAliasSet() const override {
    // When a barrier is needed make the instruction effectful by
    // giving it a "store" effect.
    if (requiresBarrier_) {
      return AliasSet::Store(AliasSet::UnboxedElement);
    }
    return AliasSet::Load(AliasSet::UnboxedElement);
  }

  bool congruentTo(const MDefinition* ins) const override {
    if (requiresBarrier_) {
      return false;
    }
    if (!ins->isLoadUnboxedScalar()) {
      return false;
    }
    const MLoadUnboxedScalar* other = ins->toLoadUnboxedScalar();
    if (storageType_ != other->storageType_) {
      return false;
    }
    if (readType_ != other->readType_) {
      return false;
    }
    if (offsetAdjustment() != other->offsetAdjustment()) {
      return false;
    }
    if (canonicalizeDoubles() != other->canonicalizeDoubles()) {
      return false;
    }
    return congruentIfOperandsEqual(other);
  }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  void computeRange(TempAllocator& alloc) override;

  bool canProduceFloat32() const override {
    return storageType_ == Scalar::Float32;
  }

  ALLOW_CLONE(MLoadUnboxedScalar)
};

// Load a value from a typed array. Out-of-bounds accesses are handled in-line.
class MLoadTypedArrayElementHole : public MBinaryInstruction,
                                   public SingleObjectPolicy::Data {
  Scalar::Type arrayType_;
  bool allowDouble_;

  MLoadTypedArrayElementHole(MDefinition* object, MDefinition* index,
                             Scalar::Type arrayType, bool allowDouble)
      : MBinaryInstruction(classOpcode, object, index),
        arrayType_(arrayType),
        allowDouble_(allowDouble) {
    setResultType(MIRType::Value);
    setMovable();
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(arrayType >= 0 && arrayType < Scalar::MaxTypedArrayViewType);
  }

 public:
  INSTRUCTION_HEADER(LoadTypedArrayElementHole)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, index))

  Scalar::Type arrayType() const { return arrayType_; }
  bool allowDouble() const { return allowDouble_; }
  bool fallible() const {
    return arrayType_ == Scalar::Uint32 && !allowDouble_;
  }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isLoadTypedArrayElementHole()) {
      return false;
    }
    const MLoadTypedArrayElementHole* other =
        ins->toLoadTypedArrayElementHole();
    if (arrayType() != other->arrayType()) {
      return false;
    }
    if (allowDouble() != other->allowDouble()) {
      return false;
    }
    return congruentIfOperandsEqual(other);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::UnboxedElement);
  }
  bool canProduceFloat32() const override {
    return arrayType_ == Scalar::Float32;
  }

  ALLOW_CLONE(MLoadTypedArrayElementHole)
};

// Base class for MIR ops that write unboxed scalar values.
class StoreUnboxedScalarBase {
  Scalar::Type writeType_;

 protected:
  explicit StoreUnboxedScalarBase(Scalar::Type writeType)
      : writeType_(writeType) {
    MOZ_ASSERT(isIntegerWrite() || isFloatWrite());
  }

 public:
  void setWriteType(Scalar::Type type) { writeType_ = type; }
  Scalar::Type writeType() const { return writeType_; }
  bool isByteWrite() const {
    return writeType_ == Scalar::Int8 || writeType_ == Scalar::Uint8 ||
           writeType_ == Scalar::Uint8Clamped;
  }
  bool isIntegerWrite() const {
    return isByteWrite() || writeType_ == Scalar::Int16 ||
           writeType_ == Scalar::Uint16 || writeType_ == Scalar::Int32 ||
           writeType_ == Scalar::Uint32;
  }
  bool isFloatWrite() const {
    return writeType_ == Scalar::Float32 || writeType_ == Scalar::Float64;
  }
};

// Store an unboxed scalar value to a typed array or other object.
class MStoreUnboxedScalar : public MTernaryInstruction,
                            public StoreUnboxedScalarBase,
                            public StoreUnboxedScalarPolicy::Data {
 public:
  enum TruncateInputKind { DontTruncateInput, TruncateInput };

 private:
  Scalar::Type storageType_;

  // Whether this store truncates out of range inputs, for use by range
  // analysis.
  TruncateInputKind truncateInput_;

  bool requiresBarrier_;
  int32_t offsetAdjustment_;

  MStoreUnboxedScalar(
      MDefinition* elements, MDefinition* index, MDefinition* value,
      Scalar::Type storageType, TruncateInputKind truncateInput,
      MemoryBarrierRequirement requiresBarrier = DoesNotRequireMemoryBarrier,
      int32_t offsetAdjustment = 0)
      : MTernaryInstruction(classOpcode, elements, index, value),
        StoreUnboxedScalarBase(storageType),
        storageType_(storageType),
        truncateInput_(truncateInput),
        requiresBarrier_(requiresBarrier == DoesRequireMemoryBarrier),
        offsetAdjustment_(offsetAdjustment) {
    if (requiresBarrier_) {
      setGuard();  // Not removable or movable
    } else {
      setMovable();
    }
    MOZ_ASSERT(IsValidElementsType(elements, offsetAdjustment));
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(storageType >= 0 && storageType < Scalar::MaxTypedArrayViewType);
  }

 public:
  INSTRUCTION_HEADER(StoreUnboxedScalar)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, value))

  Scalar::Type storageType() const { return storageType_; }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::UnboxedElement);
  }
  TruncateInputKind truncateInput() const { return truncateInput_; }
  bool requiresMemoryBarrier() const { return requiresBarrier_; }
  int32_t offsetAdjustment() const { return offsetAdjustment_; }
  TruncateKind operandTruncateKind(size_t index) const override;

  bool canConsumeFloat32(MUse* use) const override {
    return use == getUseFor(2) && writeType() == Scalar::Float32;
  }

  ALLOW_CLONE(MStoreUnboxedScalar)
};

class MStoreTypedArrayElementHole : public MQuaternaryInstruction,
                                    public StoreUnboxedScalarBase,
                                    public StoreTypedArrayHolePolicy::Data {
  MStoreTypedArrayElementHole(MDefinition* elements, MDefinition* length,
                              MDefinition* index, MDefinition* value,
                              Scalar::Type arrayType)
      : MQuaternaryInstruction(classOpcode, elements, length, index, value),
        StoreUnboxedScalarBase(arrayType) {
    setMovable();
    MOZ_ASSERT(elements->type() == MIRType::Elements);
    MOZ_ASSERT(length->type() == MIRType::Int32);
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(arrayType >= 0 && arrayType < Scalar::MaxTypedArrayViewType);
  }

 public:
  INSTRUCTION_HEADER(StoreTypedArrayElementHole)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, length), (2, index), (3, value))

  Scalar::Type arrayType() const { return writeType(); }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::UnboxedElement);
  }
  TruncateKind operandTruncateKind(size_t index) const override;

  bool canConsumeFloat32(MUse* use) const override {
    return use == getUseFor(3) && arrayType() == Scalar::Float32;
  }

  ALLOW_CLONE(MStoreTypedArrayElementHole)
};

// Compute an "effective address", i.e., a compound computation of the form:
//   base + index * scale + displacement
class MEffectiveAddress : public MBinaryInstruction, public NoTypePolicy::Data {
  MEffectiveAddress(MDefinition* base, MDefinition* index, Scale scale,
                    int32_t displacement)
      : MBinaryInstruction(classOpcode, base, index),
        scale_(scale),
        displacement_(displacement) {
    MOZ_ASSERT(base->type() == MIRType::Int32);
    MOZ_ASSERT(index->type() == MIRType::Int32);
    setMovable();
    setResultType(MIRType::Int32);
  }

  Scale scale_;
  int32_t displacement_;

 public:
  INSTRUCTION_HEADER(EffectiveAddress)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* base() const { return lhs(); }
  MDefinition* index() const { return rhs(); }
  Scale scale() const { return scale_; }
  int32_t displacement() const { return displacement_; }

  ALLOW_CLONE(MEffectiveAddress)
};

// Clamp input to range [0, 255] for Uint8ClampedArray.
class MClampToUint8 : public MUnaryInstruction, public ClampPolicy::Data {
  explicit MClampToUint8(MDefinition* input)
      : MUnaryInstruction(classOpcode, input) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ClampToUint8)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  void computeRange(TempAllocator& alloc) override;

  ALLOW_CLONE(MClampToUint8)
};

class MLoadFixedSlot : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
  size_t slot_;

 protected:
  MLoadFixedSlot(MDefinition* obj, size_t slot)
      : MUnaryInstruction(classOpcode, obj), slot_(slot) {
    setResultType(MIRType::Value);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(LoadFixedSlot)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  size_t slot() const { return slot_; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isLoadFixedSlot()) {
      return false;
    }
    if (slot() != ins->toLoadFixedSlot()->slot()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::FixedSlot);
  }

  AliasType mightAlias(const MDefinition* store) const override;

  ALLOW_CLONE(MLoadFixedSlot)
};

class MLoadFixedSlotAndUnbox : public MUnaryInstruction,
                               public SingleObjectPolicy::Data {
  size_t slot_;
  MUnbox::Mode mode_;
  BailoutKind bailoutKind_;

 protected:
  MLoadFixedSlotAndUnbox(MDefinition* obj, size_t slot, MUnbox::Mode mode,
                         MIRType type, BailoutKind kind)
      : MUnaryInstruction(classOpcode, obj),
        slot_(slot),
        mode_(mode),
        bailoutKind_(kind) {
    setResultType(type);
    setMovable();
    if (mode_ == MUnbox::TypeBarrier || mode_ == MUnbox::Fallible) {
      setGuard();
    }
  }

 public:
  INSTRUCTION_HEADER(LoadFixedSlotAndUnbox)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  size_t slot() const { return slot_; }
  MUnbox::Mode mode() const { return mode_; }
  BailoutKind bailoutKind() const { return bailoutKind_; }
  bool fallible() const { return mode_ != MUnbox::Infallible; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isLoadFixedSlotAndUnbox() ||
        slot() != ins->toLoadFixedSlotAndUnbox()->slot() ||
        mode() != ins->toLoadFixedSlotAndUnbox()->mode()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::FixedSlot);
  }

  AliasType mightAlias(const MDefinition* store) const override;

  ALLOW_CLONE(MLoadFixedSlotAndUnbox);
};

class MStoreFixedSlot
    : public MBinaryInstruction,
      public MixPolicy<SingleObjectPolicy, NoFloatPolicy<1>>::Data {
  bool needsBarrier_;
  size_t slot_;

  MStoreFixedSlot(MDefinition* obj, MDefinition* rval, size_t slot,
                  bool barrier)
      : MBinaryInstruction(classOpcode, obj, rval),
        needsBarrier_(barrier),
        slot_(slot) {}

 public:
  INSTRUCTION_HEADER(StoreFixedSlot)
  NAMED_OPERANDS((0, object), (1, value))

  static MStoreFixedSlot* New(TempAllocator& alloc, MDefinition* obj,
                              size_t slot, MDefinition* rval) {
    return new (alloc) MStoreFixedSlot(obj, rval, slot, false);
  }
  static MStoreFixedSlot* NewBarriered(TempAllocator& alloc, MDefinition* obj,
                                       size_t slot, MDefinition* rval) {
    return new (alloc) MStoreFixedSlot(obj, rval, slot, true);
  }

  size_t slot() const { return slot_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::FixedSlot);
  }
  bool needsBarrier() const { return needsBarrier_; }
  void setNeedsBarrier(bool needsBarrier = true) {
    needsBarrier_ = needsBarrier;
  }

  ALLOW_CLONE(MStoreFixedSlot)
};

struct InliningTarget {
  JSObject* target;

  // If target is a singleton, group is nullptr. If target is not a singleton,
  // this is the group we need to guard on when doing a polymorphic inlining
  // dispatch. Note that this can be different from target->group() due to
  // proto mutation.
  ObjectGroup* group;

  InliningTarget(JSObject* target, ObjectGroup* group)
      : target(target), group(group) {
    MOZ_ASSERT(target->isSingleton() == !group);
  }
};

using InliningTargets = Vector<InliningTarget, 4, JitAllocPolicy>;
using BoolVector = Vector<bool, 8, JitAllocPolicy>;

class InlinePropertyTable : public TempObject {
  struct Entry : public TempObject {
    CompilerObjectGroup group;
    CompilerFunction func;

    Entry(ObjectGroup* group, JSFunction* func) : group(group), func(func) {}
    bool appendRoots(MRootList& roots) const {
      return roots.append(group) && roots.append(func);
    }
  };

  jsbytecode* pc_;
  MResumePoint* priorResumePoint_;
  Vector<Entry*, 4, JitAllocPolicy> entries_;

 public:
  InlinePropertyTable(TempAllocator& alloc, jsbytecode* pc)
      : pc_(pc), priorResumePoint_(nullptr), entries_(alloc) {}

  void setPriorResumePoint(MResumePoint* resumePoint) {
    MOZ_ASSERT(priorResumePoint_ == nullptr);
    priorResumePoint_ = resumePoint;
  }
  bool hasPriorResumePoint() { return bool(priorResumePoint_); }
  MResumePoint* takePriorResumePoint() {
    MResumePoint* rp = priorResumePoint_;
    priorResumePoint_ = nullptr;
    return rp;
  }

  jsbytecode* pc() const { return pc_; }

  MOZ_MUST_USE bool addEntry(TempAllocator& alloc, ObjectGroup* group,
                             JSFunction* func) {
    return entries_.append(new (alloc) Entry(group, func));
  }

  size_t numEntries() const { return entries_.length(); }

  ObjectGroup* getObjectGroup(size_t i) const {
    MOZ_ASSERT(i < numEntries());
    return entries_[i]->group;
  }

  JSFunction* getFunction(size_t i) const {
    MOZ_ASSERT(i < numEntries());
    return entries_[i]->func;
  }

  bool hasFunction(JSFunction* func) const;
  bool hasObjectGroup(ObjectGroup* group) const;

  TemporaryTypeSet* buildTypeSetForFunction(TempAllocator& tempAlloc,
                                            JSFunction* func) const;

  // Remove targets that vetoed inlining from the InlinePropertyTable.
  void trimTo(const InliningTargets& targets, const BoolVector& choiceSet);

  // Ensure that the InlinePropertyTable's domain is a subset of |targets|.
  void trimToTargets(const InliningTargets& targets);

  bool appendRoots(MRootList& roots) const;
};

class MGetPropertyCache : public MBinaryInstruction,
                          public MixPolicy<BoxExceptPolicy<0, MIRType::Object>,
                                           CacheIdPolicy<1>>::Data {
  bool idempotent_ : 1;
  bool monitoredResult_ : 1;

  InlinePropertyTable* inlinePropertyTable_;

  MGetPropertyCache(MDefinition* obj, MDefinition* id, bool monitoredResult)
      : MBinaryInstruction(classOpcode, obj, id),
        idempotent_(false),
        monitoredResult_(monitoredResult),
        inlinePropertyTable_(nullptr) {
    setResultType(MIRType::Value);

    // The cache will invalidate if there are objects with e.g. lookup or
    // resolve hooks on the proto chain. setGuard ensures this check is not
    // eliminated.
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(GetPropertyCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value), (1, idval))

  InlinePropertyTable* initInlinePropertyTable(TempAllocator& alloc,
                                               jsbytecode* pc) {
    MOZ_ASSERT(inlinePropertyTable_ == nullptr);
    inlinePropertyTable_ = new (alloc) InlinePropertyTable(alloc, pc);
    return inlinePropertyTable_;
  }

  void clearInlinePropertyTable() { inlinePropertyTable_ = nullptr; }

  InlinePropertyTable* propTable() const { return inlinePropertyTable_; }

  bool idempotent() const { return idempotent_; }
  void setIdempotent() {
    idempotent_ = true;
    setMovable();
  }
  bool monitoredResult() const { return monitoredResult_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!idempotent_) {
      return false;
    }
    if (!ins->isGetPropertyCache()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override {
    if (idempotent_) {
      return AliasSet::Load(AliasSet::ObjectFields | AliasSet::FixedSlot |
                            AliasSet::DynamicSlot);
    }
    return AliasSet::Store(AliasSet::Any);
  }

  bool allowDoubleResult() const;

  bool appendRoots(MRootList& roots) const override {
    if (inlinePropertyTable_) {
      return inlinePropertyTable_->appendRoots(roots);
    }
    return true;
  }
};

class MHomeObjectSuperBase : public MUnaryInstruction,
                             public SingleObjectPolicy::Data {
  explicit MHomeObjectSuperBase(MDefinition* homeObject)
      : MUnaryInstruction(classOpcode, homeObject) {
    setResultType(MIRType::Object);
    setGuard();  // May throw if [[Prototype]] is null
  }

 public:
  INSTRUCTION_HEADER(HomeObjectSuperBase)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, homeObject))

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
};

class MGetPropSuperCache
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, BoxExceptPolicy<1, MIRType::Object>,
                       CacheIdPolicy<2>>::Data {
  MGetPropSuperCache(MDefinition* obj, MDefinition* receiver, MDefinition* id)
      : MTernaryInstruction(classOpcode, obj, receiver, id) {
    setResultType(MIRType::Value);
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(GetPropSuperCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, receiver), (2, idval))
};

struct PolymorphicEntry {
  // The group and/or shape to guard against.
  ReceiverGuard receiver;

  // The property to load, null for loads from unboxed properties.
  Shape* shape;

  bool appendRoots(MRootList& roots) const {
    return roots.append(receiver) && roots.append(shape);
  }
};

// Emit code to load a value from an object if it matches one of the receivers
// observed by the baseline IC, else bails out.
class MGetPropertyPolymorphic : public MUnaryInstruction,
                                public SingleObjectPolicy::Data {
  Vector<PolymorphicEntry, 4, JitAllocPolicy> receivers_;
  CompilerPropertyName name_;

  MGetPropertyPolymorphic(TempAllocator& alloc, MDefinition* obj,
                          PropertyName* name)
      : MUnaryInstruction(classOpcode, obj), receivers_(alloc), name_(name) {
    setGuard();
    setMovable();
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(GetPropertyPolymorphic)
  NAMED_OPERANDS((0, object))

  static MGetPropertyPolymorphic* New(TempAllocator& alloc, MDefinition* obj,
                                      PropertyName* name) {
    return new (alloc) MGetPropertyPolymorphic(alloc, obj, name);
  }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGetPropertyPolymorphic()) {
      return false;
    }
    if (name() != ins->toGetPropertyPolymorphic()->name()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  MOZ_MUST_USE bool addReceiver(const ReceiverGuard& receiver, Shape* shape) {
    PolymorphicEntry entry;
    entry.receiver = receiver;
    entry.shape = shape;
    return receivers_.append(entry);
  }
  size_t numReceivers() const { return receivers_.length(); }
  const ReceiverGuard receiver(size_t i) const {
    return receivers_[i].receiver;
  }
  Shape* shape(size_t i) const { return receivers_[i].shape; }
  PropertyName* name() const { return name_; }
  AliasSet getAliasSet() const override {
    bool hasUnboxedLoad = false;
    for (size_t i = 0; i < numReceivers(); i++) {
      if (!shape(i)) {
        hasUnboxedLoad = true;
        break;
      }
    }
    return AliasSet::Load(AliasSet::ObjectFields | AliasSet::FixedSlot |
                          AliasSet::DynamicSlot |
                          (hasUnboxedLoad ? AliasSet::UnboxedElement : 0));
  }

  AliasType mightAlias(const MDefinition* store) const override;

  bool appendRoots(MRootList& roots) const override;
};

// Emit code to store a value to an object's slots if its shape/group matches
// one of the shapes/groups observed by the baseline IC, else bails out.
class MSetPropertyPolymorphic
    : public MBinaryInstruction,
      public MixPolicy<SingleObjectPolicy, NoFloatPolicy<1>>::Data {
  Vector<PolymorphicEntry, 4, JitAllocPolicy> receivers_;
  CompilerPropertyName name_;
  bool needsBarrier_;

  MSetPropertyPolymorphic(TempAllocator& alloc, MDefinition* obj,
                          MDefinition* value, PropertyName* name)
      : MBinaryInstruction(classOpcode, obj, value),
        receivers_(alloc),
        name_(name),
        needsBarrier_(false) {}

 public:
  INSTRUCTION_HEADER(SetPropertyPolymorphic)
  NAMED_OPERANDS((0, object), (1, value))

  static MSetPropertyPolymorphic* New(TempAllocator& alloc, MDefinition* obj,
                                      MDefinition* value, PropertyName* name) {
    return new (alloc) MSetPropertyPolymorphic(alloc, obj, value, name);
  }

  MOZ_MUST_USE bool addReceiver(const ReceiverGuard& receiver, Shape* shape) {
    PolymorphicEntry entry;
    entry.receiver = receiver;
    entry.shape = shape;
    return receivers_.append(entry);
  }
  size_t numReceivers() const { return receivers_.length(); }
  const ReceiverGuard& receiver(size_t i) const {
    return receivers_[i].receiver;
  }
  Shape* shape(size_t i) const { return receivers_[i].shape; }
  PropertyName* name() const { return name_; }
  bool needsBarrier() const { return needsBarrier_; }
  void setNeedsBarrier() { needsBarrier_ = true; }
  AliasSet getAliasSet() const override {
    bool hasUnboxedStore = false;
    for (size_t i = 0; i < numReceivers(); i++) {
      if (!shape(i)) {
        hasUnboxedStore = true;
        break;
      }
    }
    return AliasSet::Store(AliasSet::ObjectFields | AliasSet::FixedSlot |
                           AliasSet::DynamicSlot |
                           (hasUnboxedStore ? AliasSet::UnboxedElement : 0));
  }
  bool appendRoots(MRootList& roots) const override;
};

class MDispatchInstruction : public MControlInstruction,
                             public SingleObjectPolicy::Data {
  // Map from JSFunction* -> MBasicBlock.
  struct Entry {
    JSFunction* func;
    // If |func| has a singleton group, |funcGroup| is null. Otherwise,
    // |funcGroup| holds the ObjectGroup for |func|, and dispatch guards
    // on the group instead of directly on the function.
    ObjectGroup* funcGroup;
    MBasicBlock* block;

    Entry(JSFunction* func, ObjectGroup* funcGroup, MBasicBlock* block)
        : func(func), funcGroup(funcGroup), block(block) {}
    bool appendRoots(MRootList& roots) const {
      return roots.append(func) && roots.append(funcGroup);
    }
  };
  Vector<Entry, 4, JitAllocPolicy> map_;

  // An optional fallback path that uses MCall.
  MBasicBlock* fallback_;
  MUse operand_;

  void initOperand(size_t index, MDefinition* operand) {
    MOZ_ASSERT(index == 0);
    operand_.init(operand, this);
  }

 public:
  NAMED_OPERANDS((0, input))
  MDispatchInstruction(TempAllocator& alloc, Opcode op, MDefinition* input)
      : MControlInstruction(op), map_(alloc), fallback_(nullptr) {
    initOperand(0, input);
  }

 protected:
  MUse* getUseFor(size_t index) final {
    MOZ_ASSERT(index == 0);
    return &operand_;
  }
  const MUse* getUseFor(size_t index) const final {
    MOZ_ASSERT(index == 0);
    return &operand_;
  }
  MDefinition* getOperand(size_t index) const final {
    MOZ_ASSERT(index == 0);
    return operand_.producer();
  }
  size_t numOperands() const final { return 1; }
  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u == getUseFor(0));
    return 0;
  }
  void replaceOperand(size_t index, MDefinition* operand) final {
    MOZ_ASSERT(index == 0);
    operand_.replaceProducer(operand);
  }

 public:
  void setSuccessor(size_t i, MBasicBlock* successor) {
    MOZ_ASSERT(i < numSuccessors());
    if (i == map_.length()) {
      fallback_ = successor;
    } else {
      map_[i].block = successor;
    }
  }
  size_t numSuccessors() const final {
    return map_.length() + (fallback_ ? 1 : 0);
  }
  void replaceSuccessor(size_t i, MBasicBlock* successor) final {
    setSuccessor(i, successor);
  }
  MBasicBlock* getSuccessor(size_t i) const final {
    MOZ_ASSERT(i < numSuccessors());
    if (i == map_.length()) {
      return fallback_;
    }
    return map_[i].block;
  }

 public:
  MOZ_MUST_USE bool addCase(JSFunction* func, ObjectGroup* funcGroup,
                            MBasicBlock* block) {
    return map_.append(Entry(func, funcGroup, block));
  }
  uint32_t numCases() const { return map_.length(); }
  JSFunction* getCase(uint32_t i) const { return map_[i].func; }
  ObjectGroup* getCaseObjectGroup(uint32_t i) const {
    return map_[i].funcGroup;
  }
  MBasicBlock* getCaseBlock(uint32_t i) const { return map_[i].block; }

  bool hasFallback() const { return bool(fallback_); }
  void addFallback(MBasicBlock* block) {
    MOZ_ASSERT(!hasFallback());
    fallback_ = block;
  }
  MBasicBlock* getFallback() const {
    MOZ_ASSERT(hasFallback());
    return fallback_;
  }
  bool appendRoots(MRootList& roots) const override;
};

// Polymorphic dispatch for inlining, keyed off incoming ObjectGroup.
class MObjectGroupDispatch : public MDispatchInstruction {
  // Map ObjectGroup (of CallProp's Target Object) -> JSFunction (yielded by the
  // CallProp).
  InlinePropertyTable* inlinePropertyTable_;

  MObjectGroupDispatch(TempAllocator& alloc, MDefinition* input,
                       InlinePropertyTable* table)
      : MDispatchInstruction(alloc, classOpcode, input),
        inlinePropertyTable_(table) {}

 public:
  INSTRUCTION_HEADER(ObjectGroupDispatch)

  static MObjectGroupDispatch* New(TempAllocator& alloc, MDefinition* ins,
                                   InlinePropertyTable* table) {
    return new (alloc) MObjectGroupDispatch(alloc, ins, table);
  }

  InlinePropertyTable* propTable() const { return inlinePropertyTable_; }
  bool appendRoots(MRootList& roots) const override;
};

// Polymorphic dispatch for inlining, keyed off incoming JSFunction*.
class MFunctionDispatch : public MDispatchInstruction {
  MFunctionDispatch(TempAllocator& alloc, MDefinition* input)
      : MDispatchInstruction(alloc, classOpcode, input) {}

 public:
  INSTRUCTION_HEADER(FunctionDispatch)

  static MFunctionDispatch* New(TempAllocator& alloc, MDefinition* ins) {
    return new (alloc) MFunctionDispatch(alloc, ins);
  }
  bool appendRoots(MRootList& roots) const override;
};

class MBindNameCache : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
  explicit MBindNameCache(MDefinition* envChain)
      : MUnaryInstruction(classOpcode, envChain) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(BindNameCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, environmentChain))
};

class MCallBindVar : public MUnaryInstruction, public SingleObjectPolicy::Data {
  explicit MCallBindVar(MDefinition* envChain)
      : MUnaryInstruction(classOpcode, envChain) {
    setResultType(MIRType::Object);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(CallBindVar)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, environmentChain))

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isCallBindVar()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Guard on an object's shape.
class MGuardShape : public MUnaryInstruction, public SingleObjectPolicy::Data {
  CompilerShape shape_;
  BailoutKind bailoutKind_;

  MGuardShape(MDefinition* obj, Shape* shape, BailoutKind bailoutKind)
      : MUnaryInstruction(classOpcode, obj),
        shape_(shape),
        bailoutKind_(bailoutKind) {
    setGuard();
    setMovable();
    setResultType(MIRType::Object);
    setResultTypeSet(obj->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(GuardShape)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  const Shape* shape() const { return shape_; }
  BailoutKind bailoutKind() const { return bailoutKind_; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGuardShape()) {
      return false;
    }
    if (shape() != ins->toGuardShape()->shape()) {
      return false;
    }
    if (bailoutKind() != ins->toGuardShape()->bailoutKind()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(shape_);
  }
};

// Bail if the object's shape or unboxed group is not in the input list.
class MGuardReceiverPolymorphic : public MUnaryInstruction,
                                  public SingleObjectPolicy::Data {
  Vector<ReceiverGuard, 4, JitAllocPolicy> receivers_;

  MGuardReceiverPolymorphic(TempAllocator& alloc, MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj), receivers_(alloc) {
    setGuard();
    setMovable();
    setResultType(MIRType::Object);
    setResultTypeSet(obj->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(GuardReceiverPolymorphic)
  NAMED_OPERANDS((0, object))

  static MGuardReceiverPolymorphic* New(TempAllocator& alloc,
                                        MDefinition* obj) {
    return new (alloc) MGuardReceiverPolymorphic(alloc, obj);
  }

  MOZ_MUST_USE bool addReceiver(const ReceiverGuard& receiver) {
    return receivers_.append(receiver);
  }
  size_t numReceivers() const { return receivers_.length(); }
  const ReceiverGuard& receiver(size_t i) const { return receivers_[i]; }

  bool congruentTo(const MDefinition* ins) const override;

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }

  bool appendRoots(MRootList& roots) const override;
};

// Guard on an object's group, inclusively or exclusively.
class MGuardObjectGroup : public MUnaryInstruction,
                          public SingleObjectPolicy::Data {
  CompilerObjectGroup group_;
  bool bailOnEquality_;
  BailoutKind bailoutKind_;

  MGuardObjectGroup(MDefinition* obj, ObjectGroup* group, bool bailOnEquality,
                    BailoutKind bailoutKind)
      : MUnaryInstruction(classOpcode, obj),
        group_(group),
        bailOnEquality_(bailOnEquality),
        bailoutKind_(bailoutKind) {
    setGuard();
    setMovable();
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(GuardObjectGroup)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  const ObjectGroup* group() const { return group_; }
  bool bailOnEquality() const { return bailOnEquality_; }
  BailoutKind bailoutKind() const { return bailoutKind_; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGuardObjectGroup()) {
      return false;
    }
    if (group() != ins->toGuardObjectGroup()->group()) {
      return false;
    }
    if (bailOnEquality() != ins->toGuardObjectGroup()->bailOnEquality()) {
      return false;
    }
    if (bailoutKind() != ins->toGuardObjectGroup()->bailoutKind()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(group_);
  }
};

// Guard on an object's identity, inclusively or exclusively.
class MGuardObjectIdentity : public MBinaryInstruction,
                             public SingleObjectPolicy::Data {
  bool bailOnEquality_;

  MGuardObjectIdentity(MDefinition* obj, MDefinition* expected,
                       bool bailOnEquality)
      : MBinaryInstruction(classOpcode, obj, expected),
        bailOnEquality_(bailOnEquality) {
    setGuard();
    setMovable();
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(GuardObjectIdentity)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, expected))

  bool bailOnEquality() const { return bailOnEquality_; }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGuardObjectIdentity()) {
      return false;
    }
    if (bailOnEquality() != ins->toGuardObjectIdentity()->bailOnEquality()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
};

// Load from vp[slot] (slots that are not inline in an object).
class MLoadSlot : public MUnaryInstruction, public SingleObjectPolicy::Data {
  uint32_t slot_;

  MLoadSlot(MDefinition* slots, uint32_t slot)
      : MUnaryInstruction(classOpcode, slots), slot_(slot) {
    setResultType(MIRType::Value);
    setMovable();
    MOZ_ASSERT(slots->type() == MIRType::Slots);
  }

 public:
  INSTRUCTION_HEADER(LoadSlot)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, slots))

  uint32_t slot() const { return slot_; }

  HashNumber valueHash() const override;
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isLoadSlot()) {
      return false;
    }
    if (slot() != ins->toLoadSlot()->slot()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }

  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override {
    MOZ_ASSERT(slots()->type() == MIRType::Slots);
    return AliasSet::Load(AliasSet::DynamicSlot);
  }
  AliasType mightAlias(const MDefinition* store) const override;

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  ALLOW_CLONE(MLoadSlot)
};

// Inline call to access a function's environment (scope chain).
class MFunctionEnvironment : public MUnaryInstruction,
                             public SingleObjectPolicy::Data {
  explicit MFunctionEnvironment(MDefinition* function)
      : MUnaryInstruction(classOpcode, function) {
    setResultType(MIRType::Object);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(FunctionEnvironment)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, function))

  MDefinition* foldsTo(TempAllocator& alloc) override;

  // A function's environment is fixed.
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Allocate a new LexicalEnvironmentObject.
class MNewLexicalEnvironmentObject : public MUnaryInstruction,
                                     public SingleObjectPolicy::Data {
  CompilerGCPointer<LexicalScope*> scope_;

  MNewLexicalEnvironmentObject(MDefinition* enclosing, LexicalScope* scope)
      : MUnaryInstruction(classOpcode, enclosing), scope_(scope) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(NewLexicalEnvironmentObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, enclosing))

  LexicalScope* scope() const { return scope_; }
  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(scope_);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Allocate a new LexicalEnvironmentObject from existing one
class MCopyLexicalEnvironmentObject : public MUnaryInstruction,
                                      public SingleObjectPolicy::Data {
  bool copySlots_;

  MCopyLexicalEnvironmentObject(MDefinition* env, bool copySlots)
      : MUnaryInstruction(classOpcode, env), copySlots_(copySlots) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(CopyLexicalEnvironmentObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, env))

  bool copySlots() const { return copySlots_; }
  bool possiblyCalls() const override { return true; }
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields | AliasSet::FixedSlot |
                          AliasSet::DynamicSlot);
  }
};

class MHomeObject : public MUnaryInstruction, public SingleObjectPolicy::Data {
  explicit MHomeObject(MDefinition* function)
      : MUnaryInstruction(classOpcode, function) {
    setResultType(MIRType::Object);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(HomeObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, function))

  // A function's [[HomeObject]] is fixed.
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

// Store to vp[slot] (slots that are not inline in an object).
class MStoreSlot : public MBinaryInstruction,
                   public MixPolicy<ObjectPolicy<0>, NoFloatPolicy<1>>::Data {
  uint32_t slot_;
  MIRType slotType_;
  bool needsBarrier_;

  MStoreSlot(MDefinition* slots, uint32_t slot, MDefinition* value,
             bool barrier)
      : MBinaryInstruction(classOpcode, slots, value),
        slot_(slot),
        slotType_(MIRType::Value),
        needsBarrier_(barrier) {
    MOZ_ASSERT(slots->type() == MIRType::Slots);
  }

 public:
  INSTRUCTION_HEADER(StoreSlot)
  NAMED_OPERANDS((0, slots), (1, value))

  static MStoreSlot* New(TempAllocator& alloc, MDefinition* slots,
                         uint32_t slot, MDefinition* value) {
    return new (alloc) MStoreSlot(slots, slot, value, false);
  }
  static MStoreSlot* NewBarriered(TempAllocator& alloc, MDefinition* slots,
                                  uint32_t slot, MDefinition* value) {
    return new (alloc) MStoreSlot(slots, slot, value, true);
  }

  uint32_t slot() const { return slot_; }
  MIRType slotType() const { return slotType_; }
  void setSlotType(MIRType slotType) {
    MOZ_ASSERT(slotType != MIRType::None);
    slotType_ = slotType;
  }
  bool needsBarrier() const { return needsBarrier_; }
  void setNeedsBarrier() { needsBarrier_ = true; }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::DynamicSlot);
  }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  ALLOW_CLONE(MStoreSlot)
};

class MGetNameCache : public MUnaryInstruction,
                      public SingleObjectPolicy::Data {
 private:
  explicit MGetNameCache(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(GetNameCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, envObj))
};

class MCallGetIntrinsicValue : public MNullaryInstruction {
  CompilerPropertyName name_;

  explicit MCallGetIntrinsicValue(PropertyName* name)
      : MNullaryInstruction(classOpcode), name_(name) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(CallGetIntrinsicValue)
  TRIVIAL_NEW_WRAPPERS

  PropertyName* name() const { return name_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(name_);
  }
};

class MSetPropertyInstruction : public MBinaryInstruction {
  CompilerPropertyName name_;
  bool strict_;

 protected:
  MSetPropertyInstruction(Opcode op, MDefinition* obj, MDefinition* value,
                          PropertyName* name, bool strict)
      : MBinaryInstruction(op, obj, value), name_(name), strict_(strict) {}

 public:
  NAMED_OPERANDS((0, object), (1, value))
  PropertyName* name() const { return name_; }
  bool strict() const { return strict_; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(name_);
  }
};

class MSetElementInstruction : public MTernaryInstruction {
  bool strict_;

 protected:
  MSetElementInstruction(Opcode op, MDefinition* object, MDefinition* index,
                         MDefinition* value, bool strict)
      : MTernaryInstruction(op, object, index, value), strict_(strict) {}

 public:
  NAMED_OPERANDS((0, object), (1, index), (2, value))
  bool strict() const { return strict_; }
};

class MDeleteProperty : public MUnaryInstruction, public BoxInputsPolicy::Data {
  CompilerPropertyName name_;
  bool strict_;

 protected:
  MDeleteProperty(MDefinition* val, PropertyName* name, bool strict)
      : MUnaryInstruction(classOpcode, val), name_(name), strict_(strict) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(DeleteProperty)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value))

  PropertyName* name() const { return name_; }
  bool strict() const { return strict_; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(name_);
  }
};

class MDeleteElement : public MBinaryInstruction, public BoxInputsPolicy::Data {
  bool strict_;

  MDeleteElement(MDefinition* value, MDefinition* index, bool strict)
      : MBinaryInstruction(classOpcode, value, index), strict_(strict) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(DeleteElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value), (1, index))

  bool strict() const { return strict_; }
};

// Note: This uses CallSetElementPolicy to always box its second input,
// ensuring we don't need two LIR instructions to lower this.
class MCallSetProperty : public MSetPropertyInstruction,
                         public CallSetElementPolicy::Data {
  MCallSetProperty(MDefinition* obj, MDefinition* value, PropertyName* name,
                   bool strict)
      : MSetPropertyInstruction(classOpcode, obj, value, name, strict) {}

 public:
  INSTRUCTION_HEADER(CallSetProperty)
  TRIVIAL_NEW_WRAPPERS

  bool possiblyCalls() const override { return true; }
};

class MSetPropertyCache : public MTernaryInstruction,
                          public MixPolicy<SingleObjectPolicy, CacheIdPolicy<1>,
                                           NoFloatPolicy<2>>::Data {
  bool strict_ : 1;
  bool needsPostBarrier_ : 1;
  bool needsTypeBarrier_ : 1;
  bool guardHoles_ : 1;

  MSetPropertyCache(MDefinition* obj, MDefinition* id, MDefinition* value,
                    bool strict, bool needsPostBarrier, bool typeBarrier,
                    bool guardHoles)
      : MTernaryInstruction(classOpcode, obj, id, value),
        strict_(strict),
        needsPostBarrier_(needsPostBarrier),
        needsTypeBarrier_(typeBarrier),
        guardHoles_(guardHoles) {}

 public:
  INSTRUCTION_HEADER(SetPropertyCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, idval), (2, value))

  bool needsPostBarrier() const { return needsPostBarrier_; }
  bool needsTypeBarrier() const { return needsTypeBarrier_; }

  bool guardHoles() const { return guardHoles_; }

  bool strict() const { return strict_; }
};

class MCallGetProperty : public MUnaryInstruction,
                         public BoxInputsPolicy::Data {
  CompilerPropertyName name_;
  bool idempotent_;

  MCallGetProperty(MDefinition* value, PropertyName* name)
      : MUnaryInstruction(classOpcode, value), name_(name), idempotent_(false) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(CallGetProperty)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value))

  PropertyName* name() const { return name_; }

  // Constructors need to perform a GetProp on the function prototype.
  // Since getters cannot be set on the prototype, fetching is non-effectful.
  // The operation may be safely repeated in case of bailout.
  void setIdempotent() { idempotent_ = true; }
  AliasSet getAliasSet() const override {
    if (!idempotent_) {
      return AliasSet::Store(AliasSet::Any);
    }
    return AliasSet::Load(AliasSet::ObjectFields | AliasSet::FixedSlot |
                          AliasSet::DynamicSlot);
  }
  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(name_);
  }
};

// Inline call to handle lhs[rhs]. The first input is a Value so that this
// instruction can handle both objects and strings.
class MCallGetElement : public MBinaryInstruction,
                        public BoxInputsPolicy::Data {
  MCallGetElement(MDefinition* lhs, MDefinition* rhs)
      : MBinaryInstruction(classOpcode, lhs, rhs) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(CallGetElement)
  TRIVIAL_NEW_WRAPPERS

  bool possiblyCalls() const override { return true; }
};

class MCallSetElement : public MSetElementInstruction,
                        public CallSetElementPolicy::Data {
  MCallSetElement(MDefinition* object, MDefinition* index, MDefinition* value,
                  bool strict)
      : MSetElementInstruction(classOpcode, object, index, value, strict) {}

 public:
  INSTRUCTION_HEADER(CallSetElement)
  TRIVIAL_NEW_WRAPPERS

  bool possiblyCalls() const override { return true; }
};

class MCallInitElementArray
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<1>,
                       BoxPolicy<2>>::Data {
  MCallInitElementArray(MDefinition* obj, MDefinition* index, MDefinition* val)
      : MTernaryInstruction(classOpcode, obj, index, val) {
    MOZ_ASSERT(index->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(CallInitElementArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, index), (2, value))

  bool possiblyCalls() const override { return true; }
};

class MSetDOMProperty : public MBinaryInstruction,
                        public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>>::Data {
  const JSJitSetterOp func_;
  Realm* setterRealm_;
  DOMObjectKind objectKind_;

  MSetDOMProperty(const JSJitSetterOp func, DOMObjectKind objectKind,
                  Realm* setterRealm, MDefinition* obj, MDefinition* val)
      : MBinaryInstruction(classOpcode, obj, val),
        func_(func),
        setterRealm_(setterRealm),
        objectKind_(objectKind) {}

 public:
  INSTRUCTION_HEADER(SetDOMProperty)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, value))

  JSJitSetterOp fun() const { return func_; }
  Realm* setterRealm() const { return setterRealm_; }
  DOMObjectKind objectKind() const { return objectKind_; }

  bool possiblyCalls() const override { return true; }
};

class MGetDOMPropertyBase : public MVariadicInstruction,
                            public ObjectPolicy<0>::Data {
  const JSJitInfo* info_;

 protected:
  MGetDOMPropertyBase(Opcode op, const JSJitInfo* jitinfo)
      : MVariadicInstruction(op), info_(jitinfo) {
    MOZ_ASSERT(jitinfo);
    MOZ_ASSERT(jitinfo->type() == JSJitInfo::Getter);

    // We are movable iff the jitinfo says we can be.
    if (isDomMovable()) {
      MOZ_ASSERT(jitinfo->aliasSet() != JSJitInfo::AliasEverything);
      setMovable();
    } else {
      // If we're not movable, that means we shouldn't be DCEd either,
      // because we might throw an exception when called, and getting rid
      // of that is observable.
      setGuard();
    }

    setResultType(MIRType::Value);
  }

  const JSJitInfo* info() const { return info_; }

  MOZ_MUST_USE bool init(TempAllocator& alloc, MDefinition* obj,
                         MDefinition* guard, MDefinition* globalGuard) {
    MOZ_ASSERT(obj);
    // guard can be null.
    // globalGuard can be null.
    size_t operandCount = 1;
    if (guard) {
      ++operandCount;
    }
    if (globalGuard) {
      ++operandCount;
    }
    if (!MVariadicInstruction::init(alloc, operandCount)) {
      return false;
    }
    initOperand(0, obj);

    size_t operandIndex = 1;
    // Pin the guard, if we have one as an operand if we want to hoist later.
    if (guard) {
      initOperand(operandIndex++, guard);
    }

    // And the same for the global guard, if we have one.
    if (globalGuard) {
      initOperand(operandIndex, globalGuard);
    }

    return true;
  }

 public:
  NAMED_OPERANDS((0, object))

  JSJitGetterOp fun() const { return info_->getter; }
  bool isInfallible() const { return info_->isInfallible; }
  bool isDomMovable() const { return info_->isMovable; }
  JSJitInfo::AliasSet domAliasSet() const { return info_->aliasSet(); }
  size_t domMemberSlotIndex() const {
    MOZ_ASSERT(info_->isAlwaysInSlot || info_->isLazilyCachedInSlot);
    return info_->slotIndex;
  }
  bool valueMayBeInSlot() const { return info_->isLazilyCachedInSlot; }

  bool baseCongruentTo(const MGetDOMPropertyBase* ins) const {
    if (!isDomMovable()) {
      return false;
    }

    // Checking the jitinfo is the same as checking the constant function
    if (!(info() == ins->info())) {
      return false;
    }

    return congruentIfOperandsEqual(ins);
  }

  AliasSet getAliasSet() const override {
    JSJitInfo::AliasSet aliasSet = domAliasSet();
    if (aliasSet == JSJitInfo::AliasNone) {
      return AliasSet::None();
    }
    if (aliasSet == JSJitInfo::AliasDOMSets) {
      return AliasSet::Load(AliasSet::DOMProperty);
    }
    MOZ_ASSERT(aliasSet == JSJitInfo::AliasEverything);
    return AliasSet::Store(AliasSet::Any);
  }
};

class MGetDOMProperty : public MGetDOMPropertyBase {
  Realm* getterRealm_;
  DOMObjectKind objectKind_;

 protected:
  MGetDOMProperty(const JSJitInfo* jitinfo, DOMObjectKind objectKind,
                  Realm* getterRealm)
      : MGetDOMPropertyBase(classOpcode, jitinfo),
        getterRealm_(getterRealm),
        objectKind_(objectKind) {}

 public:
  INSTRUCTION_HEADER(GetDOMProperty)

  static MGetDOMProperty* New(TempAllocator& alloc, const JSJitInfo* info,
                              DOMObjectKind objectKind, Realm* getterRealm,
                              MDefinition* obj, MDefinition* guard,
                              MDefinition* globalGuard) {
    auto* res = new (alloc) MGetDOMProperty(info, objectKind, getterRealm);
    if (!res || !res->init(alloc, obj, guard, globalGuard)) {
      return nullptr;
    }
    return res;
  }

  Realm* getterRealm() const { return getterRealm_; }
  DOMObjectKind objectKind() const { return objectKind_; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGetDOMProperty()) {
      return false;
    }

    if (ins->toGetDOMProperty()->getterRealm() != getterRealm()) {
      return false;
    }

    return baseCongruentTo(ins->toGetDOMProperty());
  }

  bool possiblyCalls() const override { return true; }
};

class MGetDOMMember : public MGetDOMPropertyBase {
  explicit MGetDOMMember(const JSJitInfo* jitinfo)
      : MGetDOMPropertyBase(classOpcode, jitinfo) {
    setResultType(MIRTypeFromValueType(jitinfo->returnType()));
  }

 public:
  INSTRUCTION_HEADER(GetDOMMember)

  static MGetDOMMember* New(TempAllocator& alloc, const JSJitInfo* info,
                            MDefinition* obj, MDefinition* guard,
                            MDefinition* globalGuard) {
    auto* res = new (alloc) MGetDOMMember(info);
    if (!res || !res->init(alloc, obj, guard, globalGuard)) {
      return nullptr;
    }
    return res;
  }

  bool possiblyCalls() const override { return false; }

  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGetDOMMember()) {
      return false;
    }

    return baseCongruentTo(ins->toGetDOMMember());
  }
};

class MStringLength : public MUnaryInstruction, public StringPolicy<0>::Data {
  explicit MStringLength(MDefinition* string)
      : MUnaryInstruction(classOpcode, string) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(StringLength)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, string))

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    // The string |length| property is immutable, so there is no
    // implicit dependency.
    return AliasSet::None();
  }

  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MStringLength)
};

// Inlined assembly for Math.floor(double | float32) -> int32.
class MFloor : public MUnaryInstruction, public FloatingPointPolicy<0>::Data {
  explicit MFloor(MDefinition* num) : MUnaryInstruction(classOpcode, num) {
    setResultType(MIRType::Int32);
    specialization_ = MIRType::Double;
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Floor)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MFloor)
};

// Inlined assembly version for Math.ceil(double | float32) -> int32.
class MCeil : public MUnaryInstruction, public FloatingPointPolicy<0>::Data {
  explicit MCeil(MDefinition* num) : MUnaryInstruction(classOpcode, num) {
    setResultType(MIRType::Int32);
    specialization_ = MIRType::Double;
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Ceil)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  void computeRange(TempAllocator& alloc) override;
  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MCeil)
};

// Inlined version of Math.round(double | float32) -> int32.
class MRound : public MUnaryInstruction, public FloatingPointPolicy<0>::Data {
  explicit MRound(MDefinition* num) : MUnaryInstruction(classOpcode, num) {
    setResultType(MIRType::Int32);
    specialization_ = MIRType::Double;
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Round)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MRound)
};

// Inlined version of Math.trunc(double | float32) -> int32.
class MTrunc : public MUnaryInstruction, public FloatingPointPolicy<0>::Data {
  explicit MTrunc(MDefinition* num) : MUnaryInstruction(classOpcode, num) {
    setResultType(MIRType::Int32);
    specialization_ = MIRType::Double;
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(Trunc)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MTrunc)
};

// NearbyInt rounds the floating-point input to the nearest integer, according
// to the RoundingMode.
class MNearbyInt : public MUnaryInstruction,
                   public FloatingPointPolicy<0>::Data {
  RoundingMode roundingMode_;

  explicit MNearbyInt(MDefinition* num, MIRType resultType,
                      RoundingMode roundingMode)
      : MUnaryInstruction(classOpcode, num), roundingMode_(roundingMode) {
    MOZ_ASSERT(HasAssemblerSupport(roundingMode));

    MOZ_ASSERT(IsFloatingPointType(resultType));
    setResultType(resultType);
    specialization_ = resultType;

    setMovable();
  }

 public:
  INSTRUCTION_HEADER(NearbyInt)
  TRIVIAL_NEW_WRAPPERS

  static bool HasAssemblerSupport(RoundingMode mode) {
    return Assembler::HasRoundInstruction(mode);
  }

  RoundingMode roundingMode() const { return roundingMode_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool isFloat32Commutative() const override { return true; }
  void trySpecializeFloat32(TempAllocator& alloc) override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override { return true; }
#endif

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           ins->toNearbyInt()->roundingMode() == roundingMode_;
  }

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;

  bool canRecoverOnBailout() const override {
    switch (roundingMode_) {
      case RoundingMode::Up:
      case RoundingMode::Down:
      case RoundingMode::TowardsZero:
        return true;
      default:
        return false;
    }
  }

  ALLOW_CLONE(MNearbyInt)
};

class MGetIteratorCache : public MUnaryInstruction,
                          public BoxExceptPolicy<0, MIRType::Object>::Data {
  explicit MGetIteratorCache(MDefinition* val)
      : MUnaryInstruction(classOpcode, val) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(GetIteratorCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value))
};

class MIteratorMore : public MUnaryInstruction,
                      public SingleObjectPolicy::Data {
  explicit MIteratorMore(MDefinition* iter)
      : MUnaryInstruction(classOpcode, iter) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(IteratorMore)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, iterator))
};

class MIsNoIter : public MUnaryInstruction, public NoTypePolicy::Data {
  explicit MIsNoIter(MDefinition* def) : MUnaryInstruction(classOpcode, def) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsNoIter)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MIteratorEnd : public MUnaryInstruction, public SingleObjectPolicy::Data {
  explicit MIteratorEnd(MDefinition* iter)
      : MUnaryInstruction(classOpcode, iter) {}

 public:
  INSTRUCTION_HEADER(IteratorEnd)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, iterator))
};

// Implementation for 'in' operator using instruction cache
class MInCache : public MBinaryInstruction,
                 public MixPolicy<CacheIdPolicy<0>, ObjectPolicy<1>>::Data {
  MInCache(MDefinition* key, MDefinition* obj)
      : MBinaryInstruction(classOpcode, key, obj) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(InCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, key), (1, object))
};

// Test whether the index is in the array bounds or a hole.
class MInArray : public MQuaternaryInstruction, public ObjectPolicy<3>::Data {
  bool needsHoleCheck_;
  bool needsNegativeIntCheck_;

  MInArray(MDefinition* elements, MDefinition* index, MDefinition* initLength,
           MDefinition* object, bool needsHoleCheck)
      : MQuaternaryInstruction(classOpcode, elements, index, initLength,
                               object),
        needsHoleCheck_(needsHoleCheck),
        needsNegativeIntCheck_(true) {
    setResultType(MIRType::Boolean);
    setMovable();
    MOZ_ASSERT(elements->type() == MIRType::Elements);
    MOZ_ASSERT(index->type() == MIRType::Int32);
    MOZ_ASSERT(initLength->type() == MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(InArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, initLength), (3, object))

  bool needsHoleCheck() const { return needsHoleCheck_; }
  bool needsNegativeIntCheck() const { return needsNegativeIntCheck_; }
  void collectRangeInfoPreTrunc() override;
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::Element);
  }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isInArray()) {
      return false;
    }
    const MInArray* other = ins->toInArray();
    if (needsHoleCheck() != other->needsHoleCheck()) {
      return false;
    }
    if (needsNegativeIntCheck() != other->needsNegativeIntCheck()) {
      return false;
    }
    return congruentIfOperandsEqual(other);
  }
};

class MHasOwnCache : public MBinaryInstruction,
                     public MixPolicy<BoxExceptPolicy<0, MIRType::Object>,
                                      CacheIdPolicy<1>>::Data {
  MHasOwnCache(MDefinition* obj, MDefinition* id)
      : MBinaryInstruction(classOpcode, obj, id) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(HasOwnCache)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value), (1, idval))
};

// Implementation for instanceof operator with specific rhs.
class MInstanceOf : public MUnaryInstruction, public InstanceOfPolicy::Data {
  CompilerObject protoObj_;

  MInstanceOf(MDefinition* obj, JSObject* proto)
      : MUnaryInstruction(classOpcode, obj), protoObj_(proto) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(InstanceOf)
  TRIVIAL_NEW_WRAPPERS

  JSObject* prototypeObject() { return protoObj_; }

  bool appendRoots(MRootList& roots) const override {
    return roots.append(protoObj_);
  }
};

// Implementation for instanceof operator with unknown rhs.
class MInstanceOfCache : public MBinaryInstruction,
                         public MixPolicy<BoxPolicy<0>, ObjectPolicy<1>>::Data {
  MInstanceOfCache(MDefinition* obj, MDefinition* proto)
      : MBinaryInstruction(classOpcode, obj, proto) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(InstanceOfCache)
  TRIVIAL_NEW_WRAPPERS
};

class MArgumentsLength : public MNullaryInstruction {
  MArgumentsLength() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Int32);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(ArgumentsLength)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    // Arguments |length| cannot be mutated by Ion Code.
    return AliasSet::None();
  }

  void computeRange(TempAllocator& alloc) override;

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;

  bool canRecoverOnBailout() const override { return true; }
};

// This MIR instruction is used to get an argument from the actual arguments.
class MGetFrameArgument : public MUnaryInstruction,
                          public UnboxedInt32Policy<0>::Data {
  explicit MGetFrameArgument(MDefinition* idx)
      : MUnaryInstruction(classOpcode, idx) {
    setResultType(MIRType::Value);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(GetFrameArgument)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, index))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override {
    // This instruction is never aliased, because ops like JSOp::SetArg don't
    // write to the argument frames. We create an arguments object in that case.
    return AliasSet::None();
  }
};

class MNewTarget : public MNullaryInstruction {
  MNewTarget() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Value);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(NewTarget)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MRest : public MUnaryInstruction, public UnboxedInt32Policy<0>::Data {
  unsigned numFormals_;
  CompilerGCPointer<ArrayObject*> templateObject_;

  MRest(TempAllocator& alloc, CompilerConstraintList* constraints,
        MDefinition* numActuals, unsigned numFormals,
        ArrayObject* templateObject)
      : MUnaryInstruction(classOpcode, numActuals),
        numFormals_(numFormals),
        templateObject_(templateObject) {
    setResultType(MIRType::Object);
    if (!JitOptions.warpBuilder) {
      setResultTypeSet(
          MakeSingletonTypeSet(alloc, constraints, templateObject));
    }
  }

 public:
  INSTRUCTION_HEADER(Rest)
  TRIVIAL_NEW_WRAPPERS_WITH_ALLOC
  NAMED_OPERANDS((0, numActuals))

  unsigned numFormals() const { return numFormals_; }
  ArrayObject* templateObject() const { return templateObject_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObject());
  }
};

class MFilterTypeSet : public MUnaryInstruction,
                       public FilterTypeSetPolicy::Data {
  MFilterTypeSet(MDefinition* def, TemporaryTypeSet* types)
      : MUnaryInstruction(classOpcode, def) {
    MOZ_ASSERT(!types->unknown());
    setResultType(types->getKnownMIRType());
    setResultTypeSet(types);
  }

 public:
  INSTRUCTION_HEADER(FilterTypeSet)
  TRIVIAL_NEW_WRAPPERS

  bool congruentTo(const MDefinition* def) const override { return false; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  virtual bool neverHoist() const override { return resultTypeSet()->empty(); }
  void computeRange(TempAllocator& alloc) override;

  bool isFloat32Commutative() const override {
    return IsFloatingPointType(type());
  }

  bool canProduceFloat32() const override;
  bool canConsumeFloat32(MUse* operand) const override;
  void trySpecializeFloat32(TempAllocator& alloc) override;
};

// Given a value, guard that the value is in a particular TypeSet, then returns
// that value.
class MTypeBarrier : public MUnaryInstruction, public TypeBarrierPolicy::Data {
  BarrierKind barrierKind_;

  MTypeBarrier(MDefinition* def, TemporaryTypeSet* types,
               BarrierKind kind = BarrierKind::TypeSet)
      : MUnaryInstruction(classOpcode, def), barrierKind_(kind) {
    MOZ_ASSERT(kind == BarrierKind::TypeTagOnly ||
               kind == BarrierKind::TypeSet);

    MOZ_ASSERT(!types->unknown());
    setResultType(types->getKnownMIRType());
    setResultTypeSet(types);

    setGuard();
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(TypeBarrier)
  TRIVIAL_NEW_WRAPPERS

#ifdef JS_JITSPEW
  void printOpcode(GenericPrinter& out) const override;
#endif

  bool congruentTo(const MDefinition* def) const override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  virtual bool neverHoist() const override { return resultTypeSet()->empty(); }
  BarrierKind barrierKind() const { return barrierKind_; }
  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool canRedefineInput();

  bool alwaysBails() const {
    // If mirtype of input doesn't agree with mirtype of barrier,
    // we will definitely bail.
    MIRType type = resultTypeSet()->getKnownMIRType();
    if (type == MIRType::Value) {
      return false;
    }
    if (input()->type() == MIRType::Value) {
      return false;
    }
    if (input()->type() == MIRType::ObjectOrNull) {
      // The ObjectOrNull optimization is only performed when the
      // barrier's type is MIRType::Null.
      MOZ_ASSERT(type == MIRType::Null);
      return false;
    }
    return input()->type() != type;
  }

  ALLOW_CLONE(MTypeBarrier)
};

// Given a value being written to another object, update the generational store
// buffer if the value is in the nursery and object is in the tenured heap.
class MPostWriteBarrier : public MBinaryInstruction,
                          public ObjectPolicy<0>::Data {
  MPostWriteBarrier(MDefinition* obj, MDefinition* value)
      : MBinaryInstruction(classOpcode, obj, value) {
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(PostWriteBarrier)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, value))

  AliasSet getAliasSet() const override { return AliasSet::None(); }

#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override {
    // During lowering, values that neither have object nor value MIR type
    // are ignored, thus Float32 can show up at this point without any issue.
    return use == getUseFor(1);
  }
#endif

  ALLOW_CLONE(MPostWriteBarrier)
};

// Given a value being written to another object's elements at the specified
// index, update the generational store buffer if the value is in the nursery
// and object is in the tenured heap.
class MPostWriteElementBarrier
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<2>>::Data {
  MPostWriteElementBarrier(MDefinition* obj, MDefinition* value,
                           MDefinition* index)
      : MTernaryInstruction(classOpcode, obj, value, index) {
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(PostWriteElementBarrier)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object), (1, value), (2, index))

  AliasSet getAliasSet() const override { return AliasSet::None(); }

#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override {
    // During lowering, values that neither have object nor value MIR type
    // are ignored, thus Float32 can show up at this point without any issue.
    return use == getUseFor(1);
  }
#endif

  ALLOW_CLONE(MPostWriteElementBarrier)
};

class MNewNamedLambdaObject : public MNullaryInstruction {
  CompilerGCPointer<LexicalEnvironmentObject*> templateObj_;

  explicit MNewNamedLambdaObject(LexicalEnvironmentObject* templateObj)
      : MNullaryInstruction(classOpcode), templateObj_(templateObj) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(NewNamedLambdaObject)
  TRIVIAL_NEW_WRAPPERS

  LexicalEnvironmentObject* templateObj() { return templateObj_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObj_);
  }
};

class MNewCallObject : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
 public:
  INSTRUCTION_HEADER(NewCallObject)
  TRIVIAL_NEW_WRAPPERS

  explicit MNewCallObject(MConstant* templateObj)
      : MUnaryInstruction(classOpcode, templateObj) {
    MOZ_ASSERT(!templateObject()->isSingleton());
    setResultType(MIRType::Object);
  }

  CallObject* templateObject() const {
    return &getOperand(0)->toConstant()->toObject().as<CallObject>();
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }
};

class MNewStringObject : public MUnaryInstruction,
                         public ConvertToStringPolicy<0>::Data {
  CompilerObject templateObj_;

  MNewStringObject(MDefinition* input, JSObject* templateObj)
      : MUnaryInstruction(classOpcode, input), templateObj_(templateObj) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(NewStringObject)
  TRIVIAL_NEW_WRAPPERS

  StringObject* templateObj() const;

  bool appendRoots(MRootList& roots) const override {
    return roots.append(templateObj_);
  }
};

// This is an alias for MLoadFixedSlot.
class MEnclosingEnvironment : public MLoadFixedSlot {
  explicit MEnclosingEnvironment(MDefinition* obj)
      : MLoadFixedSlot(obj, EnvironmentObject::enclosingEnvironmentSlot()) {
    setResultType(MIRType::Object);
  }

 public:
  static MEnclosingEnvironment* New(TempAllocator& alloc, MDefinition* obj) {
    return new (alloc) MEnclosingEnvironment(obj);
  }

  AliasSet getAliasSet() const override {
    // EnvironmentObject reserved slots are immutable.
    return AliasSet::None();
  }
};

// This is an element of a spaghetti stack which is used to represent the memory
// context which has to be restored in case of a bailout.
struct MStoreToRecover : public TempObject,
                         public InlineSpaghettiStackNode<MStoreToRecover> {
  MDefinition* operand;

  explicit MStoreToRecover(MDefinition* operand) : operand(operand) {}
};

using MStoresToRecoverList = InlineSpaghettiStack<MStoreToRecover>;

// A resume point contains the information needed to reconstruct the Baseline
// state from a position in the JIT. See the big comment near resumeAfter() in
// IonBuilder.cpp.
class MResumePoint final : public MNode
#ifdef DEBUG
    ,
                           public InlineForwardListNode<MResumePoint>
#endif
{
 public:
  enum Mode {
    ResumeAt,     // Resume until before the current instruction
    ResumeAfter,  // Resume after the current instruction
    Outer         // State before inlining.
  };

 private:
  friend class MBasicBlock;
  friend void AssertBasicGraphCoherency(MIRGraph& graph, bool force);

  // List of stack slots needed to reconstruct the frame corresponding to the
  // function which is compiled by IonBuilder.
  FixedList<MUse> operands_;

  // List of stores needed to reconstruct the content of objects which are
  // emulated by EmulateStateOf variants.
  MStoresToRecoverList stores_;

  jsbytecode* pc_;
  MInstruction* instruction_;
  Mode mode_;

  MResumePoint(MBasicBlock* block, jsbytecode* pc, Mode mode);
  void inherit(MBasicBlock* state);

  void setBlock(MBasicBlock* block) {
    setBlockAndKind(block, Kind::ResumePoint);
  }

  // Calling isDefinition or isResumePoint on MResumePoint is unnecessary.
  bool isDefinition() const = delete;
  bool isResumePoint() const = delete;

 protected:
  // Initializes operands_ to an empty array of a fixed length.
  // The array may then be filled in by inherit().
  MOZ_MUST_USE bool init(TempAllocator& alloc);

  void clearOperand(size_t index) {
    // FixedList doesn't initialize its elements, so do an unchecked init.
    operands_[index].initUncheckedWithoutProducer(this);
  }

  MUse* getUseFor(size_t index) override { return &operands_[index]; }
  const MUse* getUseFor(size_t index) const override {
    return &operands_[index];
  }

 public:
  static MResumePoint* New(TempAllocator& alloc, MBasicBlock* block,
                           jsbytecode* pc, Mode mode);
  static MResumePoint* New(TempAllocator& alloc, MBasicBlock* block,
                           MResumePoint* model,
                           const MDefinitionVector& operands);
  static MResumePoint* Copy(TempAllocator& alloc, MResumePoint* src);

  MBasicBlock* block() const { return resumePointBlock(); }

  size_t numAllocatedOperands() const { return operands_.length(); }
  uint32_t stackDepth() const { return numAllocatedOperands(); }
  size_t numOperands() const override { return numAllocatedOperands(); }
  size_t indexOf(const MUse* u) const final {
    MOZ_ASSERT(u >= &operands_[0]);
    MOZ_ASSERT(u <= &operands_[numOperands() - 1]);
    return u - &operands_[0];
  }
  void initOperand(size_t index, MDefinition* operand) {
    // FixedList doesn't initialize its elements, so do an unchecked init.
    operands_[index].initUnchecked(operand, this);
  }
  void replaceOperand(size_t index, MDefinition* operand) final {
    operands_[index].replaceProducer(operand);
  }

  bool isObservableOperand(MUse* u) const;
  bool isObservableOperand(size_t index) const;
  bool isRecoverableOperand(MUse* u) const;

  MDefinition* getOperand(size_t index) const override {
    return operands_[index].producer();
  }
  jsbytecode* pc() const { return pc_; }
  MResumePoint* caller() const;
  uint32_t frameCount() const {
    uint32_t count = 1;
    for (MResumePoint* it = caller(); it; it = it->caller()) {
      count++;
    }
    return count;
  }
  MInstruction* instruction() { return instruction_; }
  void setInstruction(MInstruction* ins) {
    MOZ_ASSERT(!instruction_);
    instruction_ = ins;
  }
  // Only to be used by stealResumePoint.
  void replaceInstruction(MInstruction* ins) {
    MOZ_ASSERT(instruction_);
    instruction_ = ins;
  }
  void resetInstruction() {
    MOZ_ASSERT(instruction_);
    instruction_ = nullptr;
  }
  Mode mode() const { return mode_; }

  void releaseUses() {
    for (size_t i = 0, e = numOperands(); i < e; i++) {
      if (operands_[i].hasProducer()) {
        operands_[i].releaseProducer();
      }
    }
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;

  // Register a store instruction on the current resume point. This
  // instruction would be recovered when we are bailing out. The |cache|
  // argument can be any resume point, it is used to share memory if we are
  // doing the same modification.
  void addStore(TempAllocator& alloc, MDefinition* store,
                const MResumePoint* cache = nullptr);

  MStoresToRecoverList::iterator storesBegin() const { return stores_.begin(); }
  MStoresToRecoverList::iterator storesEnd() const { return stores_.end(); }

#ifdef JS_JITSPEW
  virtual void dump(GenericPrinter& out) const override;
  virtual void dump() const override;
#endif
};

class MIsCallable : public MUnaryInstruction,
                    public BoxExceptPolicy<0, MIRType::Object>::Data {
  explicit MIsCallable(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    MOZ_ASSERT(object->type() == MIRType::Object ||
               object->type() == MIRType::Value);
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsCallable)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MIsConstructor : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
 public:
  explicit MIsConstructor(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsConstructor)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MIsObject : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MIsObject(MDefinition* object)
      : MUnaryInstruction(classOpcode, object) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MIsNullOrUndefined : public MUnaryInstruction,
                           public BoxInputsPolicy::Data {
  explicit MIsNullOrUndefined(MDefinition* value)
      : MUnaryInstruction(classOpcode, value) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsNullOrUndefined)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value))

  MDefinition* foldsTo(TempAllocator& alloc) override;

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MHasClass : public MUnaryInstruction, public SingleObjectPolicy::Data {
  const JSClass* class_;

  MHasClass(MDefinition* object, const JSClass* clasp)
      : MUnaryInstruction(classOpcode, object), class_(clasp) {
    MOZ_ASSERT(object->type() == MIRType::Object ||
               (object->type() == MIRType::Value &&
                object->mightBeType(MIRType::Object)));
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(HasClass)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  const JSClass* getClass() const { return class_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isHasClass()) {
      return false;
    }
    if (getClass() != ins->toHasClass()->getClass()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
};

class MGuardToClass : public MUnaryInstruction,
                      public SingleObjectPolicy::Data {
  const JSClass* class_;

  MGuardToClass(MDefinition* object, const JSClass* clasp)
      : MUnaryInstruction(classOpcode, object), class_(clasp) {
    MOZ_ASSERT(object->type() == MIRType::Object);
    setResultType(MIRType::Object);
    setMovable();

    // We will bail out if the class type is incorrect, so we need to ensure we
    // don't eliminate this instruction
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(GuardToClass)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  const JSClass* getClass() const { return class_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    if (!ins->isGuardToClass()) {
      return false;
    }
    if (getClass() != ins->toGuardToClass()->getClass()) {
      return false;
    }
    return congruentIfOperandsEqual(ins);
  }
};

// Note: we might call a proxy trap, so this instruction is effectful.
class MIsArray : public MUnaryInstruction,
                 public BoxExceptPolicy<0, MIRType::Object>::Data {
  explicit MIsArray(MDefinition* value)
      : MUnaryInstruction(classOpcode, value) {
    setResultType(MIRType::Boolean);
  }

 public:
  INSTRUCTION_HEADER(IsArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value))
};

class MIsTypedArray : public MUnaryInstruction,
                      public SingleObjectPolicy::Data {
  bool possiblyWrapped_;

  explicit MIsTypedArray(MDefinition* value, bool possiblyWrapped)
      : MUnaryInstruction(classOpcode, value),
        possiblyWrapped_(possiblyWrapped) {
    setResultType(MIRType::Boolean);

    if (possiblyWrapped) {
      // Proxy checks may throw, so we're neither removable nor movable.
      setGuard();
    } else {
      setMovable();
    }
  }

 public:
  INSTRUCTION_HEADER(IsTypedArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value))

  bool isPossiblyWrapped() const { return possiblyWrapped_; }
  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MObjectClassToString : public MUnaryInstruction,
                             public SingleObjectPolicy::Data {
  explicit MObjectClassToString(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setMovable();
    setResultType(MIRType::String);
  }

 public:
  INSTRUCTION_HEADER(ObjectClassToString)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }
};

class MCheckReturn : public MBinaryInstruction, public BoxInputsPolicy::Data {
  explicit MCheckReturn(MDefinition* retVal, MDefinition* thisVal)
      : MBinaryInstruction(classOpcode, retVal, thisVal) {
    setGuard();
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(CheckReturn)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, returnValue), (1, thisValue))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MCheckThis : public MUnaryInstruction, public BoxInputsPolicy::Data {
  explicit MCheckThis(MDefinition* thisVal)
      : MUnaryInstruction(classOpcode, thisVal) {
    setGuard();
    setResultType(MIRType::Value);
    setResultTypeSet(thisVal->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(CheckThis)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, thisValue))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  MDefinition* foldsTo(TempAllocator& alloc) override;
};

class MCheckThisReinit : public MUnaryInstruction,
                         public BoxInputsPolicy::Data {
  explicit MCheckThisReinit(MDefinition* thisVal)
      : MUnaryInstruction(classOpcode, thisVal) {
    setGuard();
    setResultType(MIRType::Value);
    setResultTypeSet(thisVal->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(CheckThisReinit)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, thisValue))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  MDefinition* foldsTo(TempAllocator& alloc) override;
};

// Increase the warm-up counter of the provided script upon execution and test
// if the warm-up counter surpasses the threshold. Upon hit it will recompile
// the outermost script (i.e. not the inlined script).
class MRecompileCheck : public MNullaryInstruction {
 public:
  enum class RecompileCheckType : uint8_t {
    // If we're not at the highest optimization level, keep incrementing the
    // warm-up counter for the outermost script on entry. The warmup check will
    // trigger recompilation to tier up. The lazy link mechanism will be used to
    // tier up once recompilation is done.
    OptimizationLevel,

    // If we're not at the highest optimization level, keep incrementing the
    // warm-up counter at loop edges. This check will trigger invalidation for
    // very long-running loops to ensure we still tier up even if we don't
    // invoke the lazy link stub.
    OptimizationLevelOSR,

    // If we're not at the highest optimization level, keep incrementing the
    // warm-up counter for inlined scripts. This check does not trigger any
    // recompilation or invalidation, it exists to ensure inlined scripts have
    // an accurate warm-up count.
    OptimizationLevelInlined,

    // Used at the last optimization level for callees that weren't hot enough
    // to be inlined. If a callee becomes hot enough we force recompilation of
    // the caller's Ion script.
    Inlining
  };

 private:
  JSScript* script_;
  uint32_t recompileThreshold_;
  RecompileCheckType type_;

  MRecompileCheck(JSScript* script, uint32_t recompileThreshold,
                  RecompileCheckType type)
      : MNullaryInstruction(classOpcode),
        script_(script),
        recompileThreshold_(recompileThreshold),
        type_(type) {
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(RecompileCheck)
  TRIVIAL_NEW_WRAPPERS

  JSScript* script() const { return script_; }

  uint32_t recompileThreshold() const { return recompileThreshold_; }

  bool forceInvalidation() const {
    return type_ == RecompileCheckType::OptimizationLevelOSR;
  }

  bool forceRecompilation() const {
    return type_ == RecompileCheckType::Inlining;
  }

  bool checkCounter() const {
    return type_ != RecompileCheckType::OptimizationLevelInlined;
  }

  bool increaseWarmUpCounter() const {
    return (type_ == RecompileCheckType::OptimizationLevel ||
            type_ == RecompileCheckType::OptimizationLevelInlined ||
            type_ == RecompileCheckType::OptimizationLevelOSR);
  }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MAtomicIsLockFree : public MUnaryInstruction,
                          public ConvertToInt32Policy<0>::Data {
  explicit MAtomicIsLockFree(MDefinition* value)
      : MUnaryInstruction(classOpcode, value) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(AtomicIsLockFree)
  TRIVIAL_NEW_WRAPPERS

  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  MOZ_MUST_USE bool writeRecoverData(
      CompactBufferWriter& writer) const override;
  bool canRecoverOnBailout() const override { return true; }

  ALLOW_CLONE(MAtomicIsLockFree)
};

// This applies to an object that is known to be a TypedArray, it bails out
// if the obj does not map a SharedArrayBuffer.

class MGuardSharedTypedArray : public MUnaryInstruction,
                               public SingleObjectPolicy::Data {
  explicit MGuardSharedTypedArray(MDefinition* obj)
      : MUnaryInstruction(classOpcode, obj) {
    setGuard();
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(GuardSharedTypedArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, object))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MCompareExchangeTypedArrayElement
    : public MQuaternaryInstruction,
      public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<1>,
                       TruncateToInt32Policy<2>,
                       TruncateToInt32Policy<3>>::Data {
  Scalar::Type arrayType_;

  explicit MCompareExchangeTypedArrayElement(MDefinition* elements,
                                             MDefinition* index,
                                             Scalar::Type arrayType,
                                             MDefinition* oldval,
                                             MDefinition* newval)
      : MQuaternaryInstruction(classOpcode, elements, index, oldval, newval),
        arrayType_(arrayType) {
    setGuard();  // Not removable
  }

 public:
  INSTRUCTION_HEADER(CompareExchangeTypedArrayElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, oldval), (3, newval))

  bool isByteArray() const {
    return (arrayType_ == Scalar::Int8 || arrayType_ == Scalar::Uint8);
  }
  int oldvalOperand() { return 2; }
  Scalar::Type arrayType() const { return arrayType_; }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::UnboxedElement);
  }
};

class MAtomicExchangeTypedArrayElement
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<1>,
                       TruncateToInt32Policy<2>>::Data {
  Scalar::Type arrayType_;

  MAtomicExchangeTypedArrayElement(MDefinition* elements, MDefinition* index,
                                   MDefinition* value, Scalar::Type arrayType)
      : MTernaryInstruction(classOpcode, elements, index, value),
        arrayType_(arrayType) {
    MOZ_ASSERT(arrayType <= Scalar::Uint32);
    setGuard();  // Not removable
  }

 public:
  INSTRUCTION_HEADER(AtomicExchangeTypedArrayElement)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, value))

  bool isByteArray() const {
    return (arrayType_ == Scalar::Int8 || arrayType_ == Scalar::Uint8);
  }
  Scalar::Type arrayType() const { return arrayType_; }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::UnboxedElement);
  }
};

class MAtomicTypedArrayElementBinop
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, UnboxedInt32Policy<1>,
                       TruncateToInt32Policy<2>>::Data {
 private:
  AtomicOp op_;
  Scalar::Type arrayType_;

 protected:
  explicit MAtomicTypedArrayElementBinop(AtomicOp op, MDefinition* elements,
                                         MDefinition* index,
                                         Scalar::Type arrayType,
                                         MDefinition* value)
      : MTernaryInstruction(classOpcode, elements, index, value),
        op_(op),
        arrayType_(arrayType) {
    setGuard();  // Not removable
  }

 public:
  INSTRUCTION_HEADER(AtomicTypedArrayElementBinop)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, elements), (1, index), (2, value))

  bool isByteArray() const {
    return (arrayType_ == Scalar::Int8 || arrayType_ == Scalar::Uint8);
  }
  AtomicOp operation() const { return op_; }
  Scalar::Type arrayType() const { return arrayType_; }
  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::UnboxedElement);
  }
};

class MDebugger : public MNullaryInstruction {
  MDebugger() : MNullaryInstruction(classOpcode) {}

 public:
  INSTRUCTION_HEADER(Debugger)
  TRIVIAL_NEW_WRAPPERS
};

class MCheckIsObj : public MUnaryInstruction, public BoxInputsPolicy::Data {
  uint8_t checkKind_;

  MCheckIsObj(MDefinition* toCheck, uint8_t checkKind)
      : MUnaryInstruction(classOpcode, toCheck), checkKind_(checkKind) {
    setResultType(MIRType::Value);
    setResultTypeSet(toCheck->resultTypeSet());
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(CheckIsObj)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, checkValue))

  uint8_t checkKind() const { return checkKind_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MCheckIsCallable : public MUnaryInstruction,
                         public BoxInputsPolicy::Data {
  uint8_t checkKind_;

  MCheckIsCallable(MDefinition* toCheck, uint8_t checkKind)
      : MUnaryInstruction(classOpcode, toCheck), checkKind_(checkKind) {
    setResultType(MIRType::Value);
    setResultTypeSet(toCheck->resultTypeSet());
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(CheckIsCallable)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, checkValue))

  uint8_t checkKind() const { return checkKind_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }
};

class MCheckObjCoercible : public MUnaryInstruction,
                           public BoxInputsPolicy::Data {
  explicit MCheckObjCoercible(MDefinition* toCheck)
      : MUnaryInstruction(classOpcode, toCheck) {
    setGuard();
    setResultType(MIRType::Value);
    setResultTypeSet(toCheck->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(CheckObjCoercible)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, checkValue))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  MDefinition* foldsTo(TempAllocator& alloc) override;
};

class MCheckClassHeritage : public MUnaryInstruction,
                            public BoxInputsPolicy::Data {
  explicit MCheckClassHeritage(MDefinition* heritage)
      : MUnaryInstruction(classOpcode, heritage) {
    setGuard();
    setResultType(MIRType::Value);
    setResultTypeSet(heritage->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(CheckClassHeritage)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, heritage))
};

class MDebugCheckSelfHosted : public MUnaryInstruction,
                              public BoxInputsPolicy::Data {
  explicit MDebugCheckSelfHosted(MDefinition* toCheck)
      : MUnaryInstruction(classOpcode, toCheck) {
    setGuard();
    setResultType(MIRType::Value);
    setResultTypeSet(toCheck->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(DebugCheckSelfHosted)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, checkValue))
};

class MFinishBoundFunctionInit
    : public MTernaryInstruction,
      public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1>,
                       UnboxedInt32Policy<2>>::Data {
  MFinishBoundFunctionInit(MDefinition* bound, MDefinition* target,
                           MDefinition* argCount)
      : MTernaryInstruction(classOpcode, bound, target, argCount) {}

 public:
  INSTRUCTION_HEADER(FinishBoundFunctionInit)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, bound), (1, target), (2, argCount))
};

class MIsPackedArray : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
  explicit MIsPackedArray(MDefinition* array)
      : MUnaryInstruction(classOpcode, array) {
    setResultType(MIRType::Boolean);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(IsPackedArray)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, array))

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
};

class MGetPrototypeOf : public MUnaryInstruction,
                        public SingleObjectPolicy::Data {
  explicit MGetPrototypeOf(MDefinition* target)
      : MUnaryInstruction(classOpcode, target) {
    setResultType(MIRType::Value);
    setGuard();  // May throw if target is a proxy.
  }

 public:
  INSTRUCTION_HEADER(GetPrototypeOf)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, target))
};

class MObjectWithProto : public MUnaryInstruction,
                         public BoxInputsPolicy::Data {
  explicit MObjectWithProto(MDefinition* prototype)
      : MUnaryInstruction(classOpcode, prototype) {
    setResultType(MIRType::Object);
    setGuard();  // May throw if prototype is neither an object nor null.
  }

 public:
  INSTRUCTION_HEADER(ObjectWithProto)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, prototype))

  bool possiblyCalls() const override { return true; }
};

class MFunctionProto : public MNullaryInstruction {
  explicit MFunctionProto() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Object);
  }

 public:
  INSTRUCTION_HEADER(FunctionProto)
  TRIVIAL_NEW_WRAPPERS

  bool possiblyCalls() const override { return true; }
};

class MSuperFunction : public MUnaryInstruction,
                       public SingleObjectPolicy::Data {
  explicit MSuperFunction(MDefinition* callee)
      : MUnaryInstruction(classOpcode, callee) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(SuperFunction)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, callee))

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::ObjectFields);
  }
};

class MInitHomeObject : public MBinaryInstruction,
                        public MixPolicy<ObjectPolicy<0>, BoxPolicy<1>>::Data {
  explicit MInitHomeObject(MDefinition* function, MDefinition* homeObject)
      : MBinaryInstruction(classOpcode, function, homeObject) {
    setResultType(MIRType::Object);
    setResultTypeSet(function->resultTypeSet());
  }

 public:
  INSTRUCTION_HEADER(InitHomeObject)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, function), (1, homeObject))

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::ObjectFields);
  }
};

// Flips the input's sign bit, independently of the rest of the number's
// payload. Note this is different from multiplying by minus-one, which has
// side-effects for e.g. NaNs.
class MWasmNeg : public MUnaryInstruction, public NoTypePolicy::Data {
  MWasmNeg(MDefinition* op, MIRType type) : MUnaryInstruction(classOpcode, op) {
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WasmNeg)
  TRIVIAL_NEW_WRAPPERS
};

class MWasmLoadTls : public MUnaryInstruction, public NoTypePolicy::Data {
  uint32_t offset_;
  AliasSet aliases_;

  explicit MWasmLoadTls(MDefinition* tlsPointer, uint32_t offset, MIRType type,
                        AliasSet aliases)
      : MUnaryInstruction(classOpcode, tlsPointer),
        offset_(offset),
        aliases_(aliases) {
    // Different Tls data have different alias classes and only those classes
    // are allowed.
    MOZ_ASSERT(aliases_.flags() ==
                   AliasSet::Load(AliasSet::WasmHeapMeta).flags() ||
               aliases_.flags() == AliasSet::None().flags());

    // The only types supported at the moment.
    MOZ_ASSERT(type == MIRType::Pointer || type == MIRType::Int32);

    setMovable();
    setResultType(type);
  }

 public:
  INSTRUCTION_HEADER(WasmLoadTls)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, tlsPtr))

  uint32_t offset() const { return offset_; }

  bool congruentTo(const MDefinition* ins) const override {
    return op() == ins->op() && offset() == ins->toWasmLoadTls()->offset() &&
           type() == ins->type();
  }

  HashNumber valueHash() const override {
    return addU32ToHash(HashNumber(op()), offset());
  }

  AliasSet getAliasSet() const override { return aliases_; }
};

class MWasmHeapBase : public MUnaryInstruction, public NoTypePolicy::Data {
  AliasSet aliases_;

  explicit MWasmHeapBase(MDefinition* tlsPointer, AliasSet aliases)
      : MUnaryInstruction(classOpcode, tlsPointer), aliases_(aliases) {
    setMovable();
    setResultType(MIRType::Pointer);
  }

 public:
  INSTRUCTION_HEADER(WasmHeapBase)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, tlsPtr))

  bool congruentTo(const MDefinition* ins) const override {
    return ins->isWasmHeapBase();
  }

  AliasSet getAliasSet() const override { return aliases_; }
};

class MWasmBoundsCheck : public MBinaryInstruction, public NoTypePolicy::Data {
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmBoundsCheck(MDefinition* index, MDefinition* boundsCheckLimit,
                            wasm::BytecodeOffset bytecodeOffset)
      : MBinaryInstruction(classOpcode, index, boundsCheckLimit),
        bytecodeOffset_(bytecodeOffset) {
    // Bounds check is effectful: it throws for OOB.
    setGuard();

    if (JitOptions.spectreIndexMasking) {
      setResultType(MIRType::Int32);
    }
  }

 public:
  INSTRUCTION_HEADER(WasmBoundsCheck)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, index), (1, boundsCheckLimit))

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool isRedundant() const { return !isGuard(); }

  void setRedundant() { setNotGuard(); }

  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }
};

class MWasmAddOffset : public MUnaryInstruction, public NoTypePolicy::Data {
  uint32_t offset_;
  wasm::BytecodeOffset bytecodeOffset_;

  MWasmAddOffset(MDefinition* base, uint32_t offset,
                 wasm::BytecodeOffset bytecodeOffset)
      : MUnaryInstruction(classOpcode, base),
        offset_(offset),
        bytecodeOffset_(bytecodeOffset) {
    setGuard();
    setResultType(MIRType::Int32);
  }

 public:
  INSTRUCTION_HEADER(WasmAddOffset)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, base))

  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  uint32_t offset() const { return offset_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }
};

class MWasmAlignmentCheck : public MUnaryInstruction,
                            public NoTypePolicy::Data {
  uint32_t byteSize_;
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmAlignmentCheck(MDefinition* index, uint32_t byteSize,
                               wasm::BytecodeOffset bytecodeOffset)
      : MUnaryInstruction(classOpcode, index),
        byteSize_(byteSize),
        bytecodeOffset_(bytecodeOffset) {
    MOZ_ASSERT(mozilla::IsPowerOfTwo(byteSize));
    // Alignment check is effectful: it throws for unaligned.
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(WasmAlignmentCheck)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, index))

  bool congruentTo(const MDefinition* ins) const override;

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  uint32_t byteSize() const { return byteSize_; }

  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }
};

class MWasmLoad
    : public MVariadicInstruction,  // memoryBase is nullptr on some platforms
      public NoTypePolicy::Data {
  wasm::MemoryAccessDesc access_;

  explicit MWasmLoad(const wasm::MemoryAccessDesc& access, MIRType resultType)
      : MVariadicInstruction(classOpcode), access_(access) {
    setGuard();
    setResultType(resultType);
  }

 public:
  INSTRUCTION_HEADER(WasmLoad)
  NAMED_OPERANDS((0, base), (1, memoryBase));

  static MWasmLoad* New(TempAllocator& alloc, MDefinition* memoryBase,
                        MDefinition* base, const wasm::MemoryAccessDesc& access,
                        MIRType resultType) {
    MWasmLoad* load = new (alloc) MWasmLoad(access, resultType);
    if (!load->init(alloc, 1 + !!memoryBase)) {
      return nullptr;
    }

    load->initOperand(0, base);
    if (memoryBase) {
      load->initOperand(1, memoryBase);
    }

    return load;
  }

  const wasm::MemoryAccessDesc& access() const { return access_; }

  AliasSet getAliasSet() const override {
    // When a barrier is needed, make the instruction effectful by giving
    // it a "store" effect.
    if (access_.isAtomic()) {
      return AliasSet::Store(AliasSet::WasmHeap);
    }
    return AliasSet::Load(AliasSet::WasmHeap);
  }
};

class MWasmStore : public MVariadicInstruction, public NoTypePolicy::Data {
  wasm::MemoryAccessDesc access_;

  explicit MWasmStore(const wasm::MemoryAccessDesc& access)
      : MVariadicInstruction(classOpcode), access_(access) {
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(WasmStore)
  NAMED_OPERANDS((0, base), (1, value), (2, memoryBase))

  static MWasmStore* New(TempAllocator& alloc, MDefinition* memoryBase,
                         MDefinition* base,
                         const wasm::MemoryAccessDesc& access,
                         MDefinition* value) {
    MWasmStore* store = new (alloc) MWasmStore(access);
    if (!store->init(alloc, 2 + !!memoryBase)) {
      return nullptr;
    }

    store->initOperand(0, base);
    store->initOperand(1, value);
    if (memoryBase) {
      store->initOperand(2, memoryBase);
    }

    return store;
  }

  const wasm::MemoryAccessDesc& access() const { return access_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmHeap);
  }
};

class MAsmJSMemoryAccess {
  uint32_t offset_;
  Scalar::Type accessType_;
  bool needsBoundsCheck_;

 public:
  explicit MAsmJSMemoryAccess(Scalar::Type accessType)
      : offset_(0), accessType_(accessType), needsBoundsCheck_(true) {
    MOZ_ASSERT(accessType != Scalar::Uint8Clamped);
  }

  uint32_t offset() const { return offset_; }
  uint32_t endOffset() const { return offset() + byteSize(); }
  Scalar::Type accessType() const { return accessType_; }
  unsigned byteSize() const { return TypedArrayElemSize(accessType()); }
  bool needsBoundsCheck() const { return needsBoundsCheck_; }

  wasm::MemoryAccessDesc access() const {
    return wasm::MemoryAccessDesc(accessType_, Scalar::byteSize(accessType_),
                                  offset_, wasm::BytecodeOffset());
  }

  void removeBoundsCheck() { needsBoundsCheck_ = false; }
  void setOffset(uint32_t o) { offset_ = o; }
};

class MAsmJSLoadHeap
    : public MVariadicInstruction,  // 1 plus optional memoryBase and
                                    // boundsCheckLimit
      public MAsmJSMemoryAccess,
      public NoTypePolicy::Data {
  uint32_t memoryBaseIndex_;

  explicit MAsmJSLoadHeap(uint32_t memoryBaseIndex, Scalar::Type accessType)
      : MVariadicInstruction(classOpcode),
        MAsmJSMemoryAccess(accessType),
        memoryBaseIndex_(memoryBaseIndex) {
    setResultType(ScalarTypeToMIRType(accessType));
  }

 public:
  INSTRUCTION_HEADER(AsmJSLoadHeap)

  static MAsmJSLoadHeap* New(TempAllocator& alloc, MDefinition* memoryBase,
                             MDefinition* base, MDefinition* boundsCheckLimit,
                             Scalar::Type accessType) {
    uint32_t nextIndex = 2;
    uint32_t memoryBaseIndex = memoryBase ? nextIndex++ : UINT32_MAX;

    MAsmJSLoadHeap* load =
        new (alloc) MAsmJSLoadHeap(memoryBaseIndex, accessType);
    if (!load->init(alloc, nextIndex)) {
      return nullptr;
    }

    load->initOperand(0, base);
    load->initOperand(1, boundsCheckLimit);
    if (memoryBase) {
      load->initOperand(memoryBaseIndex, memoryBase);
    }

    return load;
  }

  MDefinition* base() const { return getOperand(0); }
  void replaceBase(MDefinition* newBase) { replaceOperand(0, newBase); }
  bool hasMemoryBase() const { return memoryBaseIndex_ != UINT32_MAX; }
  MDefinition* memoryBase() const {
    MOZ_ASSERT(hasMemoryBase());
    return getOperand(memoryBaseIndex_);
  }
  MDefinition* boundsCheckLimit() const { return getOperand(1); }

  bool congruentTo(const MDefinition* ins) const override;
  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::WasmHeap);
  }
  AliasType mightAlias(const MDefinition* def) const override;
};

class MAsmJSStoreHeap
    : public MVariadicInstruction,  // 2 plus optional memoryBase and
                                    // boundsCheckLimit
      public MAsmJSMemoryAccess,
      public NoTypePolicy::Data {
  uint32_t memoryBaseIndex_;

  explicit MAsmJSStoreHeap(uint32_t memoryBaseIndex, Scalar::Type accessType)
      : MVariadicInstruction(classOpcode),
        MAsmJSMemoryAccess(accessType),
        memoryBaseIndex_(memoryBaseIndex) {}

 public:
  INSTRUCTION_HEADER(AsmJSStoreHeap)

  static MAsmJSStoreHeap* New(TempAllocator& alloc, MDefinition* memoryBase,
                              MDefinition* base, MDefinition* boundsCheckLimit,
                              Scalar::Type accessType, MDefinition* v) {
    uint32_t nextIndex = 3;
    uint32_t memoryBaseIndex = memoryBase ? nextIndex++ : UINT32_MAX;

    MAsmJSStoreHeap* store =
        new (alloc) MAsmJSStoreHeap(memoryBaseIndex, accessType);
    if (!store->init(alloc, nextIndex)) {
      return nullptr;
    }

    store->initOperand(0, base);
    store->initOperand(1, v);
    store->initOperand(2, boundsCheckLimit);
    if (memoryBase) {
      store->initOperand(memoryBaseIndex, memoryBase);
    }

    return store;
  }

  MDefinition* base() const { return getOperand(0); }
  void replaceBase(MDefinition* newBase) { replaceOperand(0, newBase); }
  MDefinition* value() const { return getOperand(1); }
  bool hasMemoryBase() const { return memoryBaseIndex_ != UINT32_MAX; }
  MDefinition* memoryBase() const {
    MOZ_ASSERT(hasMemoryBase());
    return getOperand(memoryBaseIndex_);
  }
  MDefinition* boundsCheckLimit() const { return getOperand(2); }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmHeap);
  }
};

class MWasmFence : public MNullaryInstruction {
 protected:
  MWasmFence() : MNullaryInstruction(classOpcode) { setGuard(); }

 public:
  INSTRUCTION_HEADER(WasmFence)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  ALLOW_CLONE(MWasmFence)
};

class MWasmCompareExchangeHeap : public MVariadicInstruction,
                                 public NoTypePolicy::Data {
  wasm::MemoryAccessDesc access_;
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmCompareExchangeHeap(const wasm::MemoryAccessDesc& access,
                                    wasm::BytecodeOffset bytecodeOffset)
      : MVariadicInstruction(classOpcode),
        access_(access),
        bytecodeOffset_(bytecodeOffset) {
    setGuard();  // Not removable
    setResultType(ScalarTypeToMIRType(access.type()));
  }

 public:
  INSTRUCTION_HEADER(WasmCompareExchangeHeap)

  static MWasmCompareExchangeHeap* New(TempAllocator& alloc,
                                       wasm::BytecodeOffset bytecodeOffset,
                                       MDefinition* memoryBase,
                                       MDefinition* base,
                                       const wasm::MemoryAccessDesc& access,
                                       MDefinition* oldv, MDefinition* newv,
                                       MDefinition* tls) {
    MWasmCompareExchangeHeap* cas =
        new (alloc) MWasmCompareExchangeHeap(access, bytecodeOffset);
    if (!cas->init(alloc, 4 + !!memoryBase)) {
      return nullptr;
    }
    cas->initOperand(0, base);
    cas->initOperand(1, oldv);
    cas->initOperand(2, newv);
    cas->initOperand(3, tls);
    if (memoryBase) {
      cas->initOperand(4, memoryBase);
    }
    return cas;
  }

  const wasm::MemoryAccessDesc& access() const { return access_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  MDefinition* base() const { return getOperand(0); }
  MDefinition* oldValue() const { return getOperand(1); }
  MDefinition* newValue() const { return getOperand(2); }
  MDefinition* tls() const { return getOperand(3); }
  MDefinition* memoryBase() const { return getOperand(4); }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmHeap);
  }
};

class MWasmAtomicExchangeHeap : public MVariadicInstruction,
                                public NoTypePolicy::Data {
  wasm::MemoryAccessDesc access_;
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmAtomicExchangeHeap(const wasm::MemoryAccessDesc& access,
                                   wasm::BytecodeOffset bytecodeOffset)
      : MVariadicInstruction(classOpcode),
        access_(access),
        bytecodeOffset_(bytecodeOffset) {
    setGuard();  // Not removable
    setResultType(ScalarTypeToMIRType(access.type()));
  }

 public:
  INSTRUCTION_HEADER(WasmAtomicExchangeHeap)

  static MWasmAtomicExchangeHeap* New(TempAllocator& alloc,
                                      wasm::BytecodeOffset bytecodeOffset,
                                      MDefinition* memoryBase,
                                      MDefinition* base,
                                      const wasm::MemoryAccessDesc& access,
                                      MDefinition* value, MDefinition* tls) {
    MWasmAtomicExchangeHeap* xchg =
        new (alloc) MWasmAtomicExchangeHeap(access, bytecodeOffset);
    if (!xchg->init(alloc, 3 + !!memoryBase)) {
      return nullptr;
    }

    xchg->initOperand(0, base);
    xchg->initOperand(1, value);
    xchg->initOperand(2, tls);
    if (memoryBase) {
      xchg->initOperand(3, memoryBase);
    }

    return xchg;
  }

  const wasm::MemoryAccessDesc& access() const { return access_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  MDefinition* base() const { return getOperand(0); }
  MDefinition* value() const { return getOperand(1); }
  MDefinition* tls() const { return getOperand(2); }
  MDefinition* memoryBase() const { return getOperand(3); }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmHeap);
  }
};

class MWasmAtomicBinopHeap : public MVariadicInstruction,
                             public NoTypePolicy::Data {
  AtomicOp op_;
  wasm::MemoryAccessDesc access_;
  wasm::BytecodeOffset bytecodeOffset_;

  explicit MWasmAtomicBinopHeap(AtomicOp op,
                                const wasm::MemoryAccessDesc& access,
                                wasm::BytecodeOffset bytecodeOffset)
      : MVariadicInstruction(classOpcode),
        op_(op),
        access_(access),
        bytecodeOffset_(bytecodeOffset) {
    setGuard();  // Not removable
    setResultType(ScalarTypeToMIRType(access.type()));
  }

 public:
  INSTRUCTION_HEADER(WasmAtomicBinopHeap)

  static MWasmAtomicBinopHeap* New(TempAllocator& alloc,
                                   wasm::BytecodeOffset bytecodeOffset,
                                   AtomicOp op, MDefinition* memoryBase,
                                   MDefinition* base,
                                   const wasm::MemoryAccessDesc& access,
                                   MDefinition* v, MDefinition* tls) {
    MWasmAtomicBinopHeap* binop =
        new (alloc) MWasmAtomicBinopHeap(op, access, bytecodeOffset);
    if (!binop->init(alloc, 3 + !!memoryBase)) {
      return nullptr;
    }

    binop->initOperand(0, base);
    binop->initOperand(1, v);
    binop->initOperand(2, tls);
    if (memoryBase) {
      binop->initOperand(3, memoryBase);
    }

    return binop;
  }

  AtomicOp operation() const { return op_; }
  const wasm::MemoryAccessDesc& access() const { return access_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }

  MDefinition* base() const { return getOperand(0); }
  MDefinition* value() const { return getOperand(1); }
  MDefinition* tls() const { return getOperand(2); }
  MDefinition* memoryBase() const { return getOperand(3); }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmHeap);
  }
};

class MWasmLoadGlobalVar : public MUnaryInstruction, public NoTypePolicy::Data {
  MWasmLoadGlobalVar(MIRType type, unsigned globalDataOffset, bool isConstant,
                     MDefinition* tlsPtr)
      : MUnaryInstruction(classOpcode, tlsPtr),
        globalDataOffset_(globalDataOffset),
        isConstant_(isConstant) {
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type) ||
               type == MIRType::Pointer || type == MIRType::RefOrNull);
    setResultType(type);
    setMovable();
  }

  unsigned globalDataOffset_;
  bool isConstant_;

 public:
  INSTRUCTION_HEADER(WasmLoadGlobalVar)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, tlsPtr))

  unsigned globalDataOffset() const { return globalDataOffset_; }

  HashNumber valueHash() const override;
  bool congruentTo(const MDefinition* ins) const override;
  MDefinition* foldsTo(TempAllocator& alloc) override;

  AliasSet getAliasSet() const override {
    return isConstant_ ? AliasSet::None()
                       : AliasSet::Load(AliasSet::WasmGlobalVar);
  }

  AliasType mightAlias(const MDefinition* def) const override;
};

class MWasmLoadGlobalCell : public MUnaryInstruction,
                            public NoTypePolicy::Data {
  MWasmLoadGlobalCell(MIRType type, MDefinition* cellPtr)
      : MUnaryInstruction(classOpcode, cellPtr) {
    setResultType(type);
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WasmLoadGlobalCell)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, cellPtr))

  // The default valueHash is good enough, because there are no non-operand
  // fields.
  bool congruentTo(const MDefinition* ins) const override;

  AliasSet getAliasSet() const override {
    return AliasSet::Load(AliasSet::WasmGlobalCell);
  }

  AliasType mightAlias(const MDefinition* def) const override;
};

class MWasmStoreGlobalVar : public MBinaryInstruction,
                            public NoTypePolicy::Data {
  MWasmStoreGlobalVar(unsigned globalDataOffset, MDefinition* value,
                      MDefinition* tlsPtr)
      : MBinaryInstruction(classOpcode, value, tlsPtr),
        globalDataOffset_(globalDataOffset) {}

  unsigned globalDataOffset_;

 public:
  INSTRUCTION_HEADER(WasmStoreGlobalVar)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value), (1, tlsPtr))

  unsigned globalDataOffset() const { return globalDataOffset_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmGlobalVar);
  }
};

class MWasmStoreGlobalCell : public MBinaryInstruction,
                             public NoTypePolicy::Data {
  MWasmStoreGlobalCell(MDefinition* value, MDefinition* cellPtr)
      : MBinaryInstruction(classOpcode, value, cellPtr) {}

 public:
  INSTRUCTION_HEADER(WasmStoreGlobalCell)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, value), (1, cellPtr))

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmGlobalCell);
  }
};

class MWasmStoreStackResult : public MBinaryInstruction,
                              public NoTypePolicy::Data {
  MWasmStoreStackResult(MDefinition* stackResultArea, uint32_t offset,
                        MDefinition* value)
      : MBinaryInstruction(classOpcode, stackResultArea, value),
        offset_(offset) {}

  uint32_t offset_;

 public:
  INSTRUCTION_HEADER(WasmStoreStackResult)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, stackResultArea), (1, value))

  uint32_t offset() const { return offset_; }

  AliasSet getAliasSet() const override {
    return AliasSet::Store(AliasSet::WasmStackResult);
  }
};

// Represents a known-good derived pointer into an object or memory region (in
// the most general sense) that will not move while the derived pointer is live.
// The `offset` *must* be a valid offset into the object represented by `base`;
// hence overflow in the address calculation will never be an issue.

class MWasmDerivedPointer : public MUnaryInstruction,
                            public NoTypePolicy::Data {
  MWasmDerivedPointer(MDefinition* base, size_t offset)
      : MUnaryInstruction(classOpcode, base), offset_(offset) {
    MOZ_ASSERT(offset <= INT32_MAX);
    setResultType(MIRType::Pointer);
    setMovable();
  }

  size_t offset_;

 public:
  INSTRUCTION_HEADER(WasmDerivedPointer)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, base))

  size_t offset() const { return offset_; }

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           ins->toWasmDerivedPointer()->offset() == offset();
  }

  ALLOW_CLONE(MWasmDerivedPointer)
};

class MWasmStoreRef : public MAryInstruction<3>, public NoTypePolicy::Data {
  AliasSet::Flag aliasSet_;

  MWasmStoreRef(MDefinition* tls, MDefinition* valueAddr, MDefinition* value,
                AliasSet::Flag aliasSet)
      : MAryInstruction<3>(classOpcode), aliasSet_(aliasSet) {
    MOZ_ASSERT(valueAddr->type() == MIRType::Pointer);
    MOZ_ASSERT(value->type() == MIRType::RefOrNull);
    initOperand(0, tls);
    initOperand(1, valueAddr);
    initOperand(2, value);
  }

 public:
  INSTRUCTION_HEADER(WasmStoreRef)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, tls), (1, valueAddr), (2, value))

  AliasSet getAliasSet() const override { return AliasSet::Store(aliasSet_); }
};

class MWasmParameter : public MNullaryInstruction {
  ABIArg abi_;

  MWasmParameter(ABIArg abi, MIRType mirType)
      : MNullaryInstruction(classOpcode), abi_(abi) {
    setResultType(mirType);
  }

 public:
  INSTRUCTION_HEADER(WasmParameter)
  TRIVIAL_NEW_WRAPPERS

  ABIArg abi() const { return abi_; }
};

class MWasmReturn : public MAryControlInstruction<1, 0>,
                    public NoTypePolicy::Data {
  explicit MWasmReturn(MDefinition* ins) : MAryControlInstruction(classOpcode) {
    initOperand(0, ins);
  }

 public:
  INSTRUCTION_HEADER(WasmReturn)
  TRIVIAL_NEW_WRAPPERS
};

class MWasmReturnVoid : public MAryControlInstruction<0, 0>,
                        public NoTypePolicy::Data {
  MWasmReturnVoid() : MAryControlInstruction(classOpcode) {}

 public:
  INSTRUCTION_HEADER(WasmReturnVoid)
  TRIVIAL_NEW_WRAPPERS
};

class MWasmStackArg : public MUnaryInstruction, public NoTypePolicy::Data {
  MWasmStackArg(uint32_t spOffset, MDefinition* ins)
      : MUnaryInstruction(classOpcode, ins), spOffset_(spOffset) {}

  uint32_t spOffset_;

 public:
  INSTRUCTION_HEADER(WasmStackArg)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, arg))

  uint32_t spOffset() const { return spOffset_; }
  void incrementOffset(uint32_t inc) { spOffset_ += inc; }
};

template <typename Location>
class MWasmResultBase : public MNullaryInstruction {
  Location loc_;

 protected:
  MWasmResultBase(Opcode op, MIRType type, Location loc)
      : MNullaryInstruction(op), loc_(loc) {
    setResultType(type);
    setCallResultCapture();
  }

 public:
  Location loc() { return loc_; }
};

class MWasmRegisterResult : public MWasmResultBase<Register> {
  MWasmRegisterResult(MIRType type, Register reg)
      : MWasmResultBase(classOpcode, type, reg) {}

 public:
  INSTRUCTION_HEADER(WasmRegisterResult)
  TRIVIAL_NEW_WRAPPERS
};

class MWasmFloatRegisterResult : public MWasmResultBase<FloatRegister> {
  MWasmFloatRegisterResult(MIRType type, FloatRegister reg)
      : MWasmResultBase(classOpcode, type, reg) {}

 public:
  INSTRUCTION_HEADER(WasmFloatRegisterResult)
  TRIVIAL_NEW_WRAPPERS
};

class MWasmRegister64Result : public MWasmResultBase<Register64> {
  explicit MWasmRegister64Result(Register64 reg)
      : MWasmResultBase(classOpcode, MIRType::Int64, reg) {}

 public:
  INSTRUCTION_HEADER(WasmRegister64Result)
  TRIVIAL_NEW_WRAPPERS
};

class MWasmStackResultArea : public MNullaryInstruction {
 public:
  class StackResult {
    // Offset in bytes from lowest address of stack result area.
    uint32_t offset_;
    MIRType type_;

   public:
    StackResult() : type_(MIRType::Undefined) {}
    StackResult(uint32_t offset, MIRType type) : offset_(offset), type_(type) {}

    bool initialized() const { return type_ != MIRType::Undefined; }
    uint32_t offset() const {
      MOZ_ASSERT(initialized());
      return offset_;
    }
    MIRType type() const {
      MOZ_ASSERT(initialized());
      return type_;
    }
    uint32_t endOffset() const { return offset() + MIRTypeToSize(type()); }
  };

 private:
  FixedList<StackResult> results_;
  uint32_t base_;

  explicit MWasmStackResultArea()
      : MNullaryInstruction(classOpcode), base_(UINT32_MAX) {
    setResultType(MIRType::StackResults);
  }

  void assertInitialized() const {
    MOZ_ASSERT(results_.length() != 0);
#ifdef DEBUG
    for (size_t i = 0; i < results_.length(); i++) {
      MOZ_ASSERT(results_[i].initialized());
    }
#endif
  }

  bool baseInitialized() const { return base_ != UINT32_MAX; }

 public:
  INSTRUCTION_HEADER(WasmStackResultArea)
  TRIVIAL_NEW_WRAPPERS

  MOZ_MUST_USE bool init(TempAllocator& alloc, size_t stackResultCount) {
    MOZ_ASSERT(results_.length() == 0);
    MOZ_ASSERT(stackResultCount > 0);
    if (!results_.init(alloc, stackResultCount)) {
      return false;
    }
    for (size_t n = 0; n < stackResultCount; n++) {
      results_[n] = StackResult();
    }
    return true;
  }

  size_t resultCount() const { return results_.length(); }
  const StackResult& result(size_t n) const {
    MOZ_ASSERT(results_[n].initialized());
    return results_[n];
  }
  void initResult(size_t n, const StackResult& loc) {
    MOZ_ASSERT(!results_[n].initialized());
    MOZ_ASSERT((n == 0) == (loc.offset() == 0));
    MOZ_ASSERT_IF(n > 0, loc.offset() >= result(n - 1).endOffset());
    results_[n] = loc;
  }

  uint32_t byteSize() const {
    assertInitialized();
    return result(resultCount() - 1).endOffset();
  }

  // Stack index indicating base of stack area.
  uint32_t base() const {
    MOZ_ASSERT(baseInitialized());
    return base_;
  }
  void setBase(uint32_t base) {
    MOZ_ASSERT(!baseInitialized());
    base_ = base;
    MOZ_ASSERT(baseInitialized());
  }
};

class MWasmStackResult : public MUnaryInstruction, public NoTypePolicy::Data {
  uint32_t resultIdx_;

  MWasmStackResult(MWasmStackResultArea* resultArea, size_t idx)
      : MUnaryInstruction(classOpcode, resultArea), resultIdx_(idx) {
    setResultType(result().type());
  }

 public:
  INSTRUCTION_HEADER(WasmStackResult)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, resultArea))

  const MWasmStackResultArea::StackResult& result() const {
    return resultArea()->toWasmStackResultArea()->result(resultIdx_);
  }
};

class MWasmCall final : public MVariadicInstruction, public NoTypePolicy::Data {
  wasm::CallSiteDesc desc_;
  wasm::CalleeDesc callee_;
  wasm::FailureMode builtinMethodFailureMode_;
  FixedList<AnyRegister> argRegs_;
  uint32_t stackArgAreaSizeUnaligned_;
  ABIArg instanceArg_;

  MWasmCall(const wasm::CallSiteDesc& desc, const wasm::CalleeDesc& callee,
            uint32_t stackArgAreaSizeUnaligned)
      : MVariadicInstruction(classOpcode),
        desc_(desc),
        callee_(callee),
        builtinMethodFailureMode_(wasm::FailureMode::Infallible),
        stackArgAreaSizeUnaligned_(stackArgAreaSizeUnaligned) {}

 public:
  INSTRUCTION_HEADER(WasmCall)

  struct Arg {
    AnyRegister reg;
    MDefinition* def;
    Arg(AnyRegister reg, MDefinition* def) : reg(reg), def(def) {}
  };
  typedef Vector<Arg, 8, SystemAllocPolicy> Args;

  static MWasmCall* New(TempAllocator& alloc, const wasm::CallSiteDesc& desc,
                        const wasm::CalleeDesc& callee, const Args& args,
                        uint32_t stackArgAreaSizeUnaligned,
                        MDefinition* tableIndex = nullptr);

  static MWasmCall* NewBuiltinInstanceMethodCall(
      TempAllocator& alloc, const wasm::CallSiteDesc& desc,
      const wasm::SymbolicAddress builtin, wasm::FailureMode failureMode,
      const ABIArg& instanceArg, const Args& args,
      uint32_t stackArgAreaSizeUnaligned);

  size_t numArgs() const { return argRegs_.length(); }
  AnyRegister registerForArg(size_t index) const {
    MOZ_ASSERT(index < numArgs());
    return argRegs_[index];
  }
  const wasm::CallSiteDesc& desc() const { return desc_; }
  const wasm::CalleeDesc& callee() const { return callee_; }
  wasm::FailureMode builtinMethodFailureMode() const {
    MOZ_ASSERT(callee_.which() == wasm::CalleeDesc::BuiltinInstanceMethod);
    return builtinMethodFailureMode_;
  }
  uint32_t stackArgAreaSizeUnaligned() const {
    return stackArgAreaSizeUnaligned_;
  }

  bool possiblyCalls() const override { return true; }

  const ABIArg& instanceArg() const { return instanceArg_; }
};

class MWasmSelect : public MTernaryInstruction, public NoTypePolicy::Data {
  MWasmSelect(MDefinition* trueExpr, MDefinition* falseExpr,
              MDefinition* condExpr)
      : MTernaryInstruction(classOpcode, trueExpr, falseExpr, condExpr) {
    MOZ_ASSERT(condExpr->type() == MIRType::Int32);
    MOZ_ASSERT(trueExpr->type() == falseExpr->type());
    setResultType(trueExpr->type());
    setMovable();
  }

 public:
  INSTRUCTION_HEADER(WasmSelect)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, trueExpr), (1, falseExpr), (2, condExpr))

  AliasSet getAliasSet() const override { return AliasSet::None(); }

  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  ALLOW_CLONE(MWasmSelect)
};

class MWasmReinterpret : public MUnaryInstruction, public NoTypePolicy::Data {
  MWasmReinterpret(MDefinition* val, MIRType toType)
      : MUnaryInstruction(classOpcode, val) {
    switch (val->type()) {
      case MIRType::Int32:
        MOZ_ASSERT(toType == MIRType::Float32);
        break;
      case MIRType::Float32:
        MOZ_ASSERT(toType == MIRType::Int32);
        break;
      case MIRType::Double:
        MOZ_ASSERT(toType == MIRType::Int64);
        break;
      case MIRType::Int64:
        MOZ_ASSERT(toType == MIRType::Double);
        break;
      default:
        MOZ_CRASH("unexpected reinterpret conversion");
    }
    setMovable();
    setResultType(toType);
  }

 public:
  INSTRUCTION_HEADER(WasmReinterpret)
  TRIVIAL_NEW_WRAPPERS

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins);
  }

  ALLOW_CLONE(MWasmReinterpret)
};

class MRotate : public MBinaryInstruction, public NoTypePolicy::Data {
  bool isLeftRotate_;

  MRotate(MDefinition* input, MDefinition* count, MIRType type,
          bool isLeftRotate)
      : MBinaryInstruction(classOpcode, input, count),
        isLeftRotate_(isLeftRotate) {
    setMovable();
    setResultType(type);
    // Prevent reordering.  Although there's no problem eliding call result
    // definitions, there's also no need, as they cause no codegen.
    setGuard();
  }

 public:
  INSTRUCTION_HEADER(Rotate)
  TRIVIAL_NEW_WRAPPERS
  NAMED_OPERANDS((0, input), (1, count))

  AliasSet getAliasSet() const override { return AliasSet::None(); }
  bool congruentTo(const MDefinition* ins) const override {
    return congruentIfOperandsEqual(ins) &&
           ins->toRotate()->isLeftRotate() == isLeftRotate_;
  }

  bool isLeftRotate() const { return isLeftRotate_; }

  ALLOW_CLONE(MRotate)
};

class MUnknownValue : public MNullaryInstruction {
 protected:
  MUnknownValue() : MNullaryInstruction(classOpcode) {
    setResultType(MIRType::Value);
  }

 public:
  INSTRUCTION_HEADER(UnknownValue)
  TRIVIAL_NEW_WRAPPERS
};

class MIonToWasmCall final : public MVariadicInstruction,
                             public NoTypePolicy::Data {
  CompilerGCPointer<WasmInstanceObject*> instanceObj_;
  const wasm::FuncExport& funcExport_;

  MIonToWasmCall(WasmInstanceObject* instanceObj, MIRType resultType,
                 const wasm::FuncExport& funcExport)
      : MVariadicInstruction(classOpcode),
        instanceObj_(instanceObj),
        funcExport_(funcExport) {
    setResultType(resultType);
  }

 public:
  INSTRUCTION_HEADER(IonToWasmCall);

  static MIonToWasmCall* New(TempAllocator& alloc,
                             WasmInstanceObject* instanceObj,
                             const wasm::FuncExport& funcExport);

  void initArg(size_t i, MDefinition* arg) { initOperand(i, arg); }

  WasmInstanceObject* instanceObject() const { return instanceObj_; }
  const wasm::FuncExport& funcExport() const { return funcExport_; }
  bool possiblyCalls() const override { return true; }
  bool appendRoots(MRootList& roots) const override;
#ifdef DEBUG
  bool isConsistentFloat32Use(MUse* use) const override;
#endif
};

#undef INSTRUCTION_HEADER

void MUse::init(MDefinition* producer, MNode* consumer) {
  MOZ_ASSERT(!consumer_, "Initializing MUse that already has a consumer");
  MOZ_ASSERT(!producer_, "Initializing MUse that already has a producer");
  initUnchecked(producer, consumer);
}

void MUse::initUnchecked(MDefinition* producer, MNode* consumer) {
  MOZ_ASSERT(consumer, "Initializing to null consumer");
  consumer_ = consumer;
  producer_ = producer;
  producer_->addUseUnchecked(this);
}

void MUse::initUncheckedWithoutProducer(MNode* consumer) {
  MOZ_ASSERT(consumer, "Initializing to null consumer");
  consumer_ = consumer;
  producer_ = nullptr;
}

void MUse::replaceProducer(MDefinition* producer) {
  MOZ_ASSERT(consumer_, "Resetting MUse without a consumer");
  producer_->removeUse(this);
  producer_ = producer;
  producer_->addUse(this);
}

void MUse::releaseProducer() {
  MOZ_ASSERT(consumer_, "Clearing MUse without a consumer");
  producer_->removeUse(this);
  producer_ = nullptr;
}

// Implement cast functions now that the compiler can see the inheritance.

MDefinition* MNode::toDefinition() {
  MOZ_ASSERT(isDefinition());
  return (MDefinition*)this;
}

MResumePoint* MNode::toResumePoint() {
  MOZ_ASSERT(isResumePoint());
  return (MResumePoint*)this;
}

MInstruction* MDefinition::toInstruction() {
  MOZ_ASSERT(!isPhi());
  return (MInstruction*)this;
}

const MInstruction* MDefinition::toInstruction() const {
  MOZ_ASSERT(!isPhi());
  return (const MInstruction*)this;
}

MControlInstruction* MDefinition::toControlInstruction() {
  MOZ_ASSERT(isControlInstruction());
  return (MControlInstruction*)this;
}

MConstant* MDefinition::maybeConstantValue() {
  MDefinition* op = this;
  if (op->isBox()) {
    op = op->toBox()->input();
  }
  if (op->isConstant()) {
    return op->toConstant();
  }
  return nullptr;
}

// Helper functions used to decide how to build MIR.

bool ElementAccessIsDenseNative(CompilerConstraintList* constraints,
                                MDefinition* obj, MDefinition* id);
bool ElementAccessIsTypedArray(CompilerConstraintList* constraints,
                               MDefinition* obj, MDefinition* id,
                               Scalar::Type* arrayType);
bool ElementAccessIsPacked(CompilerConstraintList* constraints,
                           MDefinition* obj);
bool ElementAccessMightBeCopyOnWrite(CompilerConstraintList* constraints,
                                     MDefinition* obj);
bool ElementAccessMightBeNonExtensible(CompilerConstraintList* constraints,
                                       MDefinition* obj);
AbortReasonOr<bool> ElementAccessHasExtraIndexedProperty(IonBuilder* builder,
                                                         MDefinition* obj);
MIRType DenseNativeElementType(CompilerConstraintList* constraints,
                               MDefinition* obj);
BarrierKind PropertyReadNeedsTypeBarrier(
    JSContext* propertycx, TempAllocator& alloc,
    CompilerConstraintList* constraints, TypeSet::ObjectKey* key,
    PropertyName* name, TemporaryTypeSet* observed, bool updateObserved);
BarrierKind PropertyReadNeedsTypeBarrier(JSContext* propertycx,
                                         TempAllocator& alloc,
                                         CompilerConstraintList* constraints,
                                         MDefinition* obj, PropertyName* name,
                                         TemporaryTypeSet* observed);
AbortReasonOr<BarrierKind> PropertyReadOnPrototypeNeedsTypeBarrier(
    IonBuilder* builder, MDefinition* obj, PropertyName* name,
    TemporaryTypeSet* observed);
bool PropertyReadIsIdempotent(CompilerConstraintList* constraints,
                              MDefinition* obj, PropertyName* name);
bool CanWriteProperty(TempAllocator& alloc, CompilerConstraintList* constraints,
                      HeapTypeSetKey property, MDefinition* value,
                      MIRType implicitType = MIRType::None);
bool PropertyWriteNeedsTypeBarrier(TempAllocator& alloc,
                                   CompilerConstraintList* constraints,
                                   MBasicBlock* current, MDefinition** pobj,
                                   PropertyName* name, MDefinition** pvalue,
                                   bool canModify,
                                   MIRType implicitType = MIRType::None);
AbortReasonOr<bool> TypeCanHaveExtraIndexedProperties(IonBuilder* builder,
                                                      TemporaryTypeSet* types);

inline MIRType MIRTypeForTypedArrayRead(Scalar::Type arrayType,
                                        bool observedDouble) {
  switch (arrayType) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Uint8Clamped:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
      return MIRType::Int32;
    case Scalar::Uint32:
      return observedDouble ? MIRType::Double : MIRType::Int32;
    case Scalar::Float32:
      return MIRType::Float32;
    case Scalar::Float64:
      return MIRType::Double;
    case Scalar::BigInt64:
    case Scalar::BigUint64:
      return MIRType::BigInt;
    default:
      break;
  }
  MOZ_CRASH("Unknown typed array type");
}

}  // namespace jit
}  // namespace js

// Specialize the AlignmentFinder class to make Result<V, E> works with abstract
// classes such as MDefinition*, and MInstruction*
namespace mozilla {

template <>
class AlignmentFinder<js::jit::MDefinition>
    : public AlignmentFinder<js::jit::MStart> {};

template <>
class AlignmentFinder<js::jit::MInstruction>
    : public AlignmentFinder<js::jit::MStart> {};

}  // namespace mozilla

#endif /* jit_MIR_h */
