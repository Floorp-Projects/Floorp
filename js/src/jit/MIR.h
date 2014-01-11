/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Everything needed to build actual MIR instructions: the actual opcodes and
 * instructions, the instruction interface, and use chains.
 */

#ifndef jit_MIR_h
#define jit_MIR_h

#include "mozilla/Array.h"

#include "jsinfer.h"

#include "jit/CompilerRoot.h"
#include "jit/FixedList.h"
#include "jit/InlineList.h"
#include "jit/IonAllocPolicy.h"
#include "jit/IonMacroAssembler.h"
#include "jit/MOpcodes.h"
#include "jit/TypePolicy.h"
#include "jit/TypeRepresentationSet.h"
#include "vm/ScopeObject.h"
#include "vm/TypedArrayObject.h"

namespace js {

class StringObject;

namespace jit {

class BaselineInspector;
class ValueNumberData;
class Range;

static const inline
MIRType MIRTypeFromValue(const js::Value &vp)
{
    if (vp.isDouble())
        return MIRType_Double;
    return MIRTypeFromValueType(vp.extractNonDoubleType());
}

#define MIR_FLAG_LIST(_)                                                        \
    _(InWorklist)                                                               \
    _(EmittedAtUses)                                                            \
    _(LoopInvariant)                                                            \
    _(Commutative)                                                              \
    _(Movable)       /* Allow LICM and GVN to move this instruction */          \
    _(Lowered)       /* (Debug only) has a virtual register */                  \
    _(Guard)         /* Not removable if uses == 0 */                           \
                                                                                \
    /* Keep the flagged instruction in resume points and do not substitute this
     * instruction by an UndefinedValue. This might be used by call inlining
     * when a function argument is not used by the inlined instructions.
     */                                                                         \
    _(ImplicitlyUsed)                                                           \
                                                                                \
    /* The instruction has been marked dead for lazy removal from resume
     * points.
     */                                                                         \
    _(Unused)                                                                   \
    /* Marks if an instruction has fewer uses than the original code.
     * E.g. UCE can remove code.
     * Every instruction where an use is/was removed from an instruction and
     * as a result the number of operands doesn't equal the original code
     * need to get marked as UseRemoved. This is important for truncation
     * analysis to know, since if all original uses are still present,
     * it can ignore resumepoints.
     * Currently this is done for every pass after IonBuilder and before
     * Truncate Doubles. So every time removeUse is called, UseRemoved needs
     * to get set.
     */                                                                         \
    _(UseRemoved)

class MDefinition;
class MInstruction;
class MBasicBlock;
class MNode;
class MUse;
class MIRGraph;
class MResumePoint;

static inline bool isOSRLikeValue (MDefinition *def);

// Represents a use of a node.
class MUse : public TempObject, public InlineListNode<MUse>
{
    friend class MDefinition;

    MDefinition *producer_; // MDefinition that is being used.
    MNode *consumer_;       // The node that is using this operand.
    uint32_t index_;        // The index of this operand in its consumer.

    MUse(MDefinition *producer, MNode *consumer, uint32_t index)
      : producer_(producer),
        consumer_(consumer),
        index_(index)
    { }

  public:
    // Default constructor for use in vectors.
    MUse()
      : producer_(nullptr), consumer_(nullptr), index_(0)
    { }

    // Set data inside the MUse.
    void set(MDefinition *producer, MNode *consumer, uint32_t index) {
        producer_ = producer;
        consumer_ = consumer;
        index_ = index;
    }

    MDefinition *producer() const {
        JS_ASSERT(producer_ != nullptr);
        return producer_;
    }
    bool hasProducer() const {
        return producer_ != nullptr;
    }
    MNode *consumer() const {
        JS_ASSERT(consumer_ != nullptr);
        return consumer_;
    }
    uint32_t index() const {
        return index_;
    }
};

typedef InlineList<MUse>::iterator MUseIterator;

// A node is an entry in the MIR graph. It has two kinds:
//   MInstruction: an instruction which appears in the IR stream.
//   MResumePoint: a list of instructions that correspond to the state of the
//              interpreter stack.
//
// Nodes can hold references to MDefinitions. Each MDefinition has a list of
// nodes holding such a reference (its use chain).
class MNode : public TempObject
{
    friend class MDefinition;

  protected:
    MBasicBlock *block_;    // Containing basic block.

  public:
    enum Kind {
        Definition,
        ResumePoint
    };

    MNode()
      : block_(nullptr)
    { }

    MNode(MBasicBlock *block)
      : block_(block)
    { }

    virtual Kind kind() const = 0;

    // Returns the definition at a given operand.
    virtual MDefinition *getOperand(size_t index) const = 0;
    virtual size_t numOperands() const = 0;

    bool isDefinition() const {
        return kind() == Definition;
    }
    bool isResumePoint() const {
        return kind() == ResumePoint;
    }
    MBasicBlock *block() const {
        return block_;
    }

    // Instructions needing to hook into type analysis should return a
    // TypePolicy.
    virtual TypePolicy *typePolicy() {
        return nullptr;
    }

    // Replaces an already-set operand during iteration over a use chain.
    MUseIterator replaceOperand(MUseIterator use, MDefinition *ins);

    // Replaces an already-set operand, updating use information.
    void replaceOperand(size_t index, MDefinition *ins);

    // Resets the operand to an uninitialized state, breaking the link
    // with the previous operand's producer.
    void discardOperand(size_t index);

    inline MDefinition *toDefinition();
    inline MResumePoint *toResumePoint();

  protected:
    // Sets an unset operand, updating use information.
    virtual void setOperand(size_t index, MDefinition *operand) = 0;

    // Gets the MUse corresponding to given operand.
    virtual MUse *getUseFor(size_t index) = 0;
};

class AliasSet {
  private:
    uint32_t flags_;

  public:
    enum Flag {
        None_             = 0,
        ObjectFields      = 1 << 0, // shape, class, slots, length etc.
        Element           = 1 << 1, // A member of obj->elements.
        DynamicSlot       = 1 << 2, // A member of obj->slots.
        FixedSlot         = 1 << 3, // A member of obj->fixedSlots().
        TypedArrayElement = 1 << 4, // A typed array element.
        DOMProperty       = 1 << 5, // A DOM property
        FrameArgument     = 1 << 6, // An argument kept on the stack frame
        AsmJSGlobalVar    = 1 << 7, // An asm.js global var
        AsmJSHeap         = 1 << 8, // An asm.js heap load
        Last              = AsmJSHeap,
        Any               = Last | (Last - 1),

        NumCategories     = 9,

        // Indicates load or store.
        Store_            = 1 << 31
    };
    AliasSet(uint32_t flags)
      : flags_(flags)
    {
        JS_STATIC_ASSERT((1 << NumCategories) - 1 == Any);
    }

  public:
    inline bool isNone() const {
        return flags_ == None_;
    }
    uint32_t flags() const {
        return flags_ & Any;
    }
    inline bool isStore() const {
        return !!(flags_ & Store_);
    }
    inline bool isLoad() const {
        return !isStore() && !isNone();
    }
    inline AliasSet operator |(const AliasSet &other) const {
        return AliasSet(flags_ | other.flags_);
    }
    inline AliasSet operator &(const AliasSet &other) const {
        return AliasSet(flags_ & other.flags_);
    }
    static AliasSet None() {
        return AliasSet(None_);
    }
    static AliasSet Load(uint32_t flags) {
        JS_ASSERT(flags && !(flags & Store_));
        return AliasSet(flags);
    }
    static AliasSet Store(uint32_t flags) {
        JS_ASSERT(flags && !(flags & Store_));
        return AliasSet(flags | Store_);
    }
};

// An MDefinition is an SSA name.
class MDefinition : public MNode
{
    friend class MBasicBlock;

  public:
    enum Opcode {
#   define DEFINE_OPCODES(op) Op_##op,
        MIR_OPCODE_LIST(DEFINE_OPCODES)
#   undef DEFINE_OPCODES
        Op_Invalid
    };

  private:
    InlineList<MUse> uses_;        // Use chain.
    uint32_t id_;                  // Instruction ID, which after block re-ordering
                                   // is sorted within a basic block.
    ValueNumberData *valueNumber_; // The instruction's value number (see GVN for details in use)
    Range *range_;                 // Any computed range for this def.
    MIRType resultType_;           // Representation of result type.
    types::TemporaryTypeSet *resultTypeSet_; // Optional refinement of the result type.
    uint32_t flags_;                 // Bit flags.
    union {
        MDefinition *dependency_;  // Implicit dependency (store, call, etc.) of this instruction.
                                   // Used by alias analysis, GVN and LICM.
        uint32_t virtualRegister_;   // Used by lowering to map definitions to virtual registers.
    };

    // Track bailouts by storing the current pc in MIR instruction. Also used
    // for profiling and keeping track of what the last known pc was.
    jsbytecode *trackedPc_;

  private:
    enum Flag {
        None = 0,
#   define DEFINE_FLAG(flag) flag,
        MIR_FLAG_LIST(DEFINE_FLAG)
#   undef DEFINE_FLAG
        Total
    };

    bool hasFlags(uint32_t flags) const {
        return (flags_ & flags) == flags;
    }
    void removeFlags(uint32_t flags) {
        flags_ &= ~flags;
    }
    void setFlags(uint32_t flags) {
        flags_ |= flags;
    }

  protected:
    virtual void setBlock(MBasicBlock *block) {
        block_ = block;
    }

  public:
    MDefinition()
      : id_(0),
        valueNumber_(nullptr),
        range_(nullptr),
        resultType_(MIRType_None),
        resultTypeSet_(nullptr),
        flags_(0),
        dependency_(nullptr),
        trackedPc_(nullptr)
    { }

    virtual Opcode op() const = 0;
    virtual const char *opName() const = 0;
    void printName(FILE *fp) const;
    static void PrintOpcodeName(FILE *fp, Opcode op);
    virtual void printOpcode(FILE *fp) const;
    void dump(FILE *fp) const;
    void dump() const;

    // For LICM.
    virtual bool neverHoist() const { return false; }

    // Also for LICM. Test whether this definition is likely to be a call, which
    // would clobber all or many of the floating-point registers, such that
    // hoisting floating-point constants out of containing loops isn't likely to
    // be worthwhile.
    virtual bool possiblyCalls() const { return false; }

    void setTrackedPc(jsbytecode *pc) {
        trackedPc_ = pc;
    }

    jsbytecode *trackedPc() {
        return trackedPc_;
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
    Range *range() const {
        JS_ASSERT(type() != MIRType_None);
        return range_;
    }
    void setRange(Range *range) {
        JS_ASSERT(type() != MIRType_None);
        range_ = range;
    }

    virtual HashNumber valueHash() const;
    virtual bool congruentTo(MDefinition *ins) const {
        return false;
    }
    bool congruentIfOperandsEqual(MDefinition *ins) const;
    virtual MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    virtual void analyzeEdgeCasesForward();
    virtual void analyzeEdgeCasesBackward();

    virtual bool truncate();
    virtual bool isOperandTruncated(size_t index) const;

    // Compute an absolute or symbolic range for the value of this node.
    virtual void computeRange(TempAllocator &alloc) {
    }

    // Collect information from the pre-truncated ranges.
    virtual void collectRangeInfoPreTrunc() {
    }

    MNode::Kind kind() const {
        return MNode::Definition;
    }

    uint32_t id() const {
        JS_ASSERT(block_);
        return id_;
    }
    void setId(uint32_t id) {
        id_ = id;
    }

    uint32_t valueNumber() const;
    void setValueNumber(uint32_t vn);
    ValueNumberData *valueNumberData() {
        return valueNumber_;
    }
    void clearValueNumberData() {
        valueNumber_ = nullptr;
    }
    void setValueNumberData(ValueNumberData *vn) {
        JS_ASSERT(valueNumber_ == nullptr);
        valueNumber_ = vn;
    }
#define FLAG_ACCESSOR(flag) \
    bool is##flag() const {\
        return hasFlags(1 << flag);\
    }\
    void set##flag() {\
        JS_ASSERT(!hasFlags(1 << flag));\
        setFlags(1 << flag);\
    }\
    void setNot##flag() {\
        JS_ASSERT(hasFlags(1 << flag));\
        removeFlags(1 << flag);\
    }\
    void set##flag##Unchecked() {\
        setFlags(1 << flag);\
    }

    MIR_FLAG_LIST(FLAG_ACCESSOR)
#undef FLAG_ACCESSOR

    // Return the type of this value. This may be speculative, and enforced
    // dynamically with the use of bailout checks. If all the bailout checks
    // pass, the value will have this type.
    //
    // Unless this is an MUrsh that has bailouts disabled, which, as a special
    // case, may return a value in (INT32_MAX,UINT32_MAX] even when its type()
    // is MIRType_Int32.
    MIRType type() const {
        return resultType_;
    }

    types::TemporaryTypeSet *resultTypeSet() const {
        return resultTypeSet_;
    }
    bool emptyResultTypeSet() const;

    bool mightBeType(MIRType type) const {
        JS_ASSERT(type != MIRType_Value);

        if (type == this->type())
            return true;

        if (MIRType_Value != this->type())
            return false;

        return !resultTypeSet() || resultTypeSet()->mightBeType(ValueTypeFromMIRType(type));
    }

    // Float32 specialization operations (see big comment in IonAnalysis before the Float32
    // specialization algorithm).
    virtual bool isFloat32Commutative() const { return false; }
    virtual bool canProduceFloat32() const { return false; }
    virtual bool canConsumeFloat32() const { return false; }
    virtual void trySpecializeFloat32(TempAllocator &alloc) {}
#ifdef DEBUG
    // Used during the pass that checks that Float32 flow into valid MDefinitions
    virtual bool isConsistentFloat32Use() const {
        return type() == MIRType_Float32 || canConsumeFloat32();
    }
#endif

    // Returns the beginning of this definition's use chain.
    MUseIterator usesBegin() const {
        return uses_.begin();
    }

    // Returns the end of this definition's use chain.
    MUseIterator usesEnd() const {
        return uses_.end();
    }

    bool canEmitAtUses() const {
        return !isEmittedAtUses();
    }

    // Removes a use at the given position
    MUseIterator removeUse(MUseIterator use);
    void removeUse(MUse *use) {
        uses_.remove(use);
    }

    // Number of uses of this instruction.
    size_t useCount() const;

    // Number of uses of this instruction.
    // (only counting MDefinitions, ignoring MResumePoints)
    size_t defUseCount() const;

    // Test whether this MDefinition has exactly one use.
    bool hasOneUse() const;

    // Test whether this MDefinition has exactly one use.
    // (only counting MDefinitions, ignoring MResumePoints)
    bool hasOneDefUse() const;

    // Test whether this MDefinition has at least one use.
    // (only counting MDefinitions, ignoring MResumePoints)
    bool hasDefUses() const;

    bool hasUses() const {
        return !uses_.empty();
    }

    virtual bool isControlInstruction() const {
        return false;
    }

    void addUse(MUse *use) {
        uses_.pushFront(use);
    }
    void replaceAllUsesWith(MDefinition *dom);

    // Mark this instruction as having replaced all uses of ins, as during GVN,
    // returning false if the replacement should not be performed. For use when
    // GVN eliminates instructions which are not equivalent to one another.
    virtual bool updateForReplacement(MDefinition *ins) {
        return true;
    }

    void setVirtualRegister(uint32_t vreg) {
        virtualRegister_ = vreg;
#ifdef DEBUG
        setLoweredUnchecked();
#endif
    }
    uint32_t virtualRegister() const {
        JS_ASSERT(isLowered());
        return virtualRegister_;
    }

  public:
    // Opcode testing and casts.
#   define OPCODE_CASTS(opcode)                                             \
    bool is##opcode() const {                                               \
        return op() == Op_##opcode;                                         \
    }                                                                       \
    inline M##opcode *to##opcode();                                         \
    inline const M##opcode *to##opcode() const;
    MIR_OPCODE_LIST(OPCODE_CASTS)
#   undef OPCODE_CASTS

    inline MInstruction *toInstruction();
    bool isInstruction() const {
        return !isPhi();
    }

    void setResultType(MIRType type) {
        resultType_ = type;
    }
    void setResultTypeSet(types::TemporaryTypeSet *types) {
        resultTypeSet_ = types;
    }

    MDefinition *dependency() const {
        return dependency_;
    }
    void setDependency(MDefinition *dependency) {
        dependency_ = dependency;
    }
    virtual AliasSet getAliasSet() const {
        // Instructions are effectful by default.
        return AliasSet::Store(AliasSet::Any);
    }
    bool isEffectful() const {
        return getAliasSet().isStore();
    }
    virtual bool mightAlias(MDefinition *store) {
        // Return whether this load may depend on the specified store, given
        // that the alias sets intersect. This may be refined to exclude
        // possible aliasing in cases where alias set flags are too imprecise.
        JS_ASSERT(!isEffectful() && store->isEffectful());
        JS_ASSERT(getAliasSet().flags() & store->getAliasSet().flags());
        return true;
    }
};

// An MUseDefIterator walks over uses in a definition, skipping any use that is
// not a definition. Items from the use list must not be deleted during
// iteration.
class MUseDefIterator
{
    MDefinition *def_;
    MUseIterator current_;

    MUseIterator search(MUseIterator start) {
        MUseIterator i(start);
        for (; i != def_->usesEnd(); i++) {
            if (i->consumer()->isDefinition())
                return i;
        }
        return def_->usesEnd();
    }

  public:
    MUseDefIterator(MDefinition *def)
      : def_(def),
        current_(search(def->usesBegin()))
    { }

    operator bool() const {
        return current_ != def_->usesEnd();
    }
    MUseDefIterator operator ++(int) {
        MUseDefIterator old(*this);
        if (current_ != def_->usesEnd())
            current_++;
        current_ = search(current_);
        return old;
    }
    MUse *use() const {
        return *current_;
    }
    MDefinition *def() const {
        return current_->consumer()->toDefinition();
    }
    size_t index() const {
        return current_->index();
    }
};

// An instruction is an SSA name that is inserted into a basic block's IR
// stream.
class MInstruction
  : public MDefinition,
    public InlineListNode<MInstruction>
{
    MResumePoint *resumePoint_;

  public:
    MInstruction()
      : resumePoint_(nullptr)
    { }

    virtual bool accept(MInstructionVisitor *visitor) = 0;

    void setResumePoint(MResumePoint *resumePoint) {
        JS_ASSERT(!resumePoint_);
        resumePoint_ = resumePoint;
    }
    MResumePoint *resumePoint() const {
        return resumePoint_;
    }
};

#define INSTRUCTION_HEADER(opcode)                                          \
    Opcode op() const {                                                     \
        return MDefinition::Op_##opcode;                                    \
    }                                                                       \
    const char *opName() const {                                            \
        return #opcode;                                                     \
    }                                                                       \
    bool accept(MInstructionVisitor *visitor) {                             \
        return visitor->visit##opcode(this);                                \
    }

template <size_t Arity>
class MAryInstruction : public MInstruction
{
  protected:
    mozilla::Array<MUse, Arity> operands_;

    void setOperand(size_t index, MDefinition *operand) MOZ_FINAL MOZ_OVERRIDE {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }

    MUse *getUseFor(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        return &operands_[index];
    }

  public:
    MDefinition *getOperand(size_t index) const MOZ_FINAL MOZ_OVERRIDE {
        return operands_[index].producer();
    }
    size_t numOperands() const MOZ_FINAL MOZ_OVERRIDE {
        return Arity;
    }
};

class MNullaryInstruction : public MAryInstruction<0>
{ };

class MUnaryInstruction : public MAryInstruction<1>
{
  protected:
    MUnaryInstruction(MDefinition *ins)
    {
        setOperand(0, ins);
    }

  public:
    MDefinition *input() const {
        return getOperand(0);
    }
};

class MBinaryInstruction : public MAryInstruction<2>
{
  protected:
    MBinaryInstruction(MDefinition *left, MDefinition *right)
    {
        setOperand(0, left);
        setOperand(1, right);
    }

  public:
    MDefinition *lhs() const {
        return getOperand(0);
    }
    MDefinition *rhs() const {
        return getOperand(1);
    }

  protected:
    HashNumber valueHash() const
    {
        MDefinition *lhs = getOperand(0);
        MDefinition *rhs = getOperand(1);

        return op() ^ lhs->valueNumber() ^ rhs->valueNumber();
    }
    void swapOperands() {
        MDefinition *temp = getOperand(0);
        replaceOperand(0, getOperand(1));
        replaceOperand(1, temp);
    }

    bool congruentTo(MDefinition *ins) const
    {
        if (op() != ins->op())
            return false;

        if (type() != ins->type())
            return false;

        if (isEffectful() || ins->isEffectful())
            return false;

        MDefinition *left = getOperand(0);
        MDefinition *right = getOperand(1);
        MDefinition *tmp;

        if (isCommutative() && left->valueNumber() > right->valueNumber()) {
            tmp = right;
            right = left;
            left = tmp;
        }

        MBinaryInstruction *bi = static_cast<MBinaryInstruction *>(ins);
        MDefinition *insLeft = bi->getOperand(0);
        MDefinition *insRight = bi->getOperand(1);
        if (isCommutative() && insLeft->valueNumber() > insRight->valueNumber()) {
            tmp = insRight;
            insRight = insLeft;
            insLeft = tmp;
        }

        return (left->valueNumber() == insLeft->valueNumber()) &&
               (right->valueNumber() == insRight->valueNumber());
    }

    // Return true if the operands to this instruction are both unsigned,
    // in which case any wrapping operands were replaced with the underlying
    // int32 operands.
    bool tryUseUnsignedOperands();
};

class MTernaryInstruction : public MAryInstruction<3>
{
  protected:
    MTernaryInstruction(MDefinition *first, MDefinition *second, MDefinition *third)
    {
        setOperand(0, first);
        setOperand(1, second);
        setOperand(2, third);
    }

  protected:
    HashNumber valueHash() const
    {
        MDefinition *first = getOperand(0);
        MDefinition *second = getOperand(1);
        MDefinition *third = getOperand(2);

        return op() ^ first->valueNumber() ^ second->valueNumber() ^ third->valueNumber();
    }

    bool congruentTo(MDefinition *ins) const
    {
        if (op() != ins->op())
            return false;

        if (type() != ins->type())
            return false;

        if (isEffectful() || ins->isEffectful())
            return false;

        MTernaryInstruction *ter = static_cast<MTernaryInstruction *>(ins);
        MDefinition *first = getOperand(0);
        MDefinition *second = getOperand(1);
        MDefinition *third = getOperand(2);
        MDefinition *insFirst = ter->getOperand(0);
        MDefinition *insSecond = ter->getOperand(1);
        MDefinition *insThird = ter->getOperand(2);

        return first->valueNumber() == insFirst->valueNumber() &&
               second->valueNumber() == insSecond->valueNumber() &&
               third->valueNumber() == insThird->valueNumber();
    }
};

class MQuaternaryInstruction : public MAryInstruction<4>
{
  protected:
    MQuaternaryInstruction(MDefinition *first, MDefinition *second,
                           MDefinition *third, MDefinition *fourth)
    {
        setOperand(0, first);
        setOperand(1, second);
        setOperand(2, third);
        setOperand(3, fourth);
    }

  protected:
    HashNumber valueHash() const
    {
        MDefinition *first = getOperand(0);
        MDefinition *second = getOperand(1);
        MDefinition *third = getOperand(2);
        MDefinition *fourth = getOperand(3);

        return op() ^ first->valueNumber() ^ second->valueNumber() ^
                      third->valueNumber() ^ fourth->valueNumber();
    }

    bool congruentTo(MDefinition *ins) const
    {
        if (op() != ins->op())
            return false;

        if (type() != ins->type())
            return false;

        if (isEffectful() || ins->isEffectful())
            return false;

        MQuaternaryInstruction *qua = static_cast<MQuaternaryInstruction *>(ins);
        MDefinition *first = getOperand(0);
        MDefinition *second = getOperand(1);
        MDefinition *third = getOperand(2);
        MDefinition *fourth = getOperand(3);
        MDefinition *insFirst = qua->getOperand(0);
        MDefinition *insSecond = qua->getOperand(1);
        MDefinition *insThird = qua->getOperand(2);
        MDefinition *insFourth = qua->getOperand(3);

        return first->valueNumber() == insFirst->valueNumber() &&
               second->valueNumber() == insSecond->valueNumber() &&
               third->valueNumber() == insThird->valueNumber() &&
               fourth->valueNumber() == insFourth->valueNumber();
    }
};

// Generates an LSnapshot without further effect.
class MStart : public MNullaryInstruction
{
  public:
    enum StartType {
        StartType_Default,
        StartType_Osr
    };

  private:
    StartType startType_;

  private:
    MStart(StartType startType)
      : startType_(startType)
    { }

  public:
    INSTRUCTION_HEADER(Start)
    static MStart *New(TempAllocator &alloc, StartType startType) {
        return new(alloc) MStart(startType);
    }

    StartType startType() {
        return startType_;
    }
};

// Instruction marking on entrypoint for on-stack replacement.
// OSR may occur at loop headers (at JSOP_TRACE).
// There is at most one MOsrEntry per MIRGraph.
class MOsrEntry : public MNullaryInstruction
{
  protected:
    MOsrEntry() {
        setResultType(MIRType_Pointer);
    }

  public:
    INSTRUCTION_HEADER(OsrEntry)
    static MOsrEntry *New(TempAllocator &alloc) {
        return new(alloc) MOsrEntry;
    }
};

// No-op instruction. This cannot be moved or eliminated, and is intended for
// anchoring resume points at arbitrary points in a block.
class MNop : public MNullaryInstruction
{
  protected:
    MNop() {
    }

  public:
    INSTRUCTION_HEADER(Nop)
    static MNop *New(TempAllocator &alloc) {
        return new(alloc) MNop();
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// A constant js::Value.
class MConstant : public MNullaryInstruction
{
    Value value_;

  protected:
    MConstant(const Value &v, types::CompilerConstraintList *constraints);

  public:
    INSTRUCTION_HEADER(Constant)
    static MConstant *New(TempAllocator &alloc, const Value &v,
                          types::CompilerConstraintList *constraints = nullptr);
    static MConstant *NewAsmJS(TempAllocator &alloc, const Value &v, MIRType type);

    const js::Value &value() const {
        return value_;
    }
    const js::Value *vp() const {
        return &value_;
    }
    const bool valueToBoolean() const {
        // A hack to avoid this wordy pattern everywhere in the JIT.
        return ToBoolean(HandleValue::fromMarkedLocation(&value_));
    }

    void printOpcode(FILE *fp) const;

    HashNumber valueHash() const;
    bool congruentTo(MDefinition *ins) const;

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    bool updateForReplacement(MDefinition *def) {
        MConstant *c = def->toConstant();
        // During constant folding, we don't want to replace a float32
        // value by a double value.
        if (type() == MIRType_Float32)
            return c->type() == MIRType_Float32;
        if (type() == MIRType_Double)
            return c->type() != MIRType_Float32;
        return true;
    }

    void computeRange(TempAllocator &alloc);
    bool truncate();

    bool canProduceFloat32() const;
};

class MParameter : public MNullaryInstruction
{
    int32_t index_;

  public:
    static const int32_t THIS_SLOT = -1;

    MParameter(int32_t index, types::TemporaryTypeSet *types)
      : index_(index)
    {
        setResultType(MIRType_Value);
        setResultTypeSet(types);
    }

  public:
    INSTRUCTION_HEADER(Parameter)
    static MParameter *New(TempAllocator &alloc, int32_t index, types::TemporaryTypeSet *types);

    int32_t index() const {
        return index_;
    }
    void printOpcode(FILE *fp) const;

    HashNumber valueHash() const;
    bool congruentTo(MDefinition *ins) const;
};

class MCallee : public MNullaryInstruction
{
  public:
    MCallee()
    {
        setResultType(MIRType_Object);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Callee)

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }

    static MCallee *New(TempAllocator &alloc) {
        return new(alloc) MCallee();
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MControlInstruction : public MInstruction
{
  public:
    MControlInstruction()
    { }

    virtual size_t numSuccessors() const = 0;
    virtual MBasicBlock *getSuccessor(size_t i) const = 0;
    virtual void replaceSuccessor(size_t i, MBasicBlock *successor) = 0;

    bool isControlInstruction() const {
        return true;
    }

    void printOpcode(FILE *fp) const;
};

class MTableSwitch MOZ_FINAL
  : public MControlInstruction,
    public NoFloatPolicy<0>
{
    // The successors of the tableswitch
    // - First successor = the default case
    // - Successor 2 and higher = the cases sorted on case index.
    Vector<MBasicBlock*, 0, IonAllocPolicy> successors_;
    Vector<size_t, 0, IonAllocPolicy> cases_;

    // Contains the blocks/cases that still need to get build
    Vector<MBasicBlock*, 0, IonAllocPolicy> blocks_;

    MUse operand_;
    int32_t low_;
    int32_t high_;

    MTableSwitch(TempAllocator &alloc, MDefinition *ins,
                 int32_t low, int32_t high)
      : successors_(alloc),
        cases_(alloc),
        blocks_(alloc),
        low_(low),
        high_(high)
    {
        setOperand(0, ins);
    }

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index == 0);
        operand_.set(operand, this, index);
        operand->addUse(&operand_);
    }

    MUse *getUseFor(size_t index) {
        JS_ASSERT(index == 0);
        return &operand_;
    }

  public:
    INSTRUCTION_HEADER(TableSwitch)
    static MTableSwitch *New(TempAllocator &alloc, MDefinition *ins, int32_t low, int32_t high);

    size_t numSuccessors() const {
        return successors_.length();
    }

    size_t addSuccessor(MBasicBlock *successor) {
        JS_ASSERT(successors_.length() < (size_t)(high_ - low_ + 2));
        JS_ASSERT(!successors_.empty());
        successors_.append(successor);
        return successors_.length() - 1;
    }

    MBasicBlock *getSuccessor(size_t i) const {
        JS_ASSERT(i < numSuccessors());
        return successors_[i];
    }

    void replaceSuccessor(size_t i, MBasicBlock *successor) {
        JS_ASSERT(i < numSuccessors());
        successors_[i] = successor;
    }

    MBasicBlock** blocks() {
        return &blocks_[0];
    }

    size_t numBlocks() const {
        return blocks_.length();
    }

    int32_t low() const {
        return low_;
    }

    int32_t high() const {
        return high_;
    }

    MBasicBlock *getDefault() const {
        return getSuccessor(0);
    }

    MBasicBlock *getCase(size_t i) const {
        return getSuccessor(cases_[i]);
    }

    size_t numCases() const {
        return high() - low() + 1;
    }

    size_t addDefault(MBasicBlock *block) {
        JS_ASSERT(successors_.empty());
        successors_.append(block);
        return 0;
    }

    void addCase(size_t successorIndex) {
        cases_.append(successorIndex);
    }

    MBasicBlock *getBlock(size_t i) const {
        JS_ASSERT(i < numBlocks());
        return blocks_[i];
    }

    void addBlock(MBasicBlock *block) {
        blocks_.append(block);
    }

    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index == 0);
        return operand_.producer();
    }

    size_t numOperands() const {
        return 1;
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

template <size_t Arity, size_t Successors>
class MAryControlInstruction : public MControlInstruction
{
    mozilla::Array<MUse, Arity> operands_;
    mozilla::Array<MBasicBlock *, Successors> successors_;

  protected:
    void setOperand(size_t index, MDefinition *operand) MOZ_FINAL MOZ_OVERRIDE {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }
    void setSuccessor(size_t index, MBasicBlock *successor) {
        successors_[index] = successor;
    }

    MUse *getUseFor(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        return &operands_[index];
    }

  public:
    MDefinition *getOperand(size_t index) const MOZ_FINAL MOZ_OVERRIDE {
        return operands_[index].producer();
    }
    size_t numOperands() const MOZ_FINAL MOZ_OVERRIDE {
        return Arity;
    }
    size_t numSuccessors() const MOZ_FINAL MOZ_OVERRIDE {
        return Successors;
    }
    MBasicBlock *getSuccessor(size_t i) const MOZ_FINAL MOZ_OVERRIDE {
        return successors_[i];
    }
    void replaceSuccessor(size_t i, MBasicBlock *succ) MOZ_FINAL MOZ_OVERRIDE {
        successors_[i] = succ;
    }
};

// Jump to the start of another basic block.
class MGoto : public MAryControlInstruction<0, 1>
{
    MGoto(MBasicBlock *target) {
        setSuccessor(0, target);
    }

  public:
    INSTRUCTION_HEADER(Goto)
    static MGoto *New(TempAllocator &alloc, MBasicBlock *target);

    MBasicBlock *target() {
        return getSuccessor(0);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

enum BranchDirection {
    FALSE_BRANCH,
    TRUE_BRANCH
};

static inline BranchDirection
NegateBranchDirection(BranchDirection dir)
{
    return (dir == FALSE_BRANCH) ? TRUE_BRANCH : FALSE_BRANCH;
}

// Tests if the input instruction evaluates to true or false, and jumps to the
// start of a corresponding basic block.
class MTest
  : public MAryControlInstruction<1, 2>,
    public TestPolicy
{
    bool operandMightEmulateUndefined_;

    MTest(MDefinition *ins, MBasicBlock *if_true, MBasicBlock *if_false)
      : operandMightEmulateUndefined_(true)
    {
        setOperand(0, ins);
        setSuccessor(0, if_true);
        setSuccessor(1, if_false);
    }

  public:
    INSTRUCTION_HEADER(Test)
    static MTest *New(TempAllocator &alloc, MDefinition *ins,
                      MBasicBlock *ifTrue, MBasicBlock *ifFalse);

    MDefinition *input() const {
        return getOperand(0);
    }
    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
    MBasicBlock *branchSuccessor(BranchDirection dir) const {
        return (dir == TRUE_BRANCH) ? ifTrue() : ifFalse();
    }
    TypePolicy *typePolicy() {
        return this;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void infer();
    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    void markOperandCantEmulateUndefined() {
        operandMightEmulateUndefined_ = false;
    }
    bool operandMightEmulateUndefined() const {
        return operandMightEmulateUndefined_;
    }
#ifdef DEBUG
    bool isConsistentFloat32Use() const {
        return true;
    }
#endif
};

// Returns from this function to the previous caller.
class MReturn
  : public MAryControlInstruction<1, 0>,
    public BoxInputsPolicy
{
    MReturn(MDefinition *ins) {
        setOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(Return)
    static MReturn *New(TempAllocator &alloc, MDefinition *ins) {
        return new(alloc) MReturn(ins);
    }

    MDefinition *input() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MThrow
  : public MAryControlInstruction<1, 0>,
    public BoxInputsPolicy
{
    MThrow(MDefinition *ins) {
        setOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(Throw)
    static MThrow *New(TempAllocator &alloc, MDefinition *ins) {
        return new(alloc) MThrow(ins);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Fabricate a type set containing only the type of the specified object.
types::TemporaryTypeSet *
MakeSingletonTypeSet(types::CompilerConstraintList *constraints, JSObject *obj);

void
MergeTypes(MIRType *ptype, types::TemporaryTypeSet **ptypeSet,
           MIRType newType, types::TemporaryTypeSet *newTypeSet);

class MNewArray : public MNullaryInstruction
{
  public:
    enum AllocatingBehaviour {
        NewArray_Allocating,
        NewArray_Unallocating
    };

  private:
    // Number of space to allocate for the array.
    uint32_t count_;
    // Template for the created object.
    CompilerRootObject templateObject_;
    gc::InitialHeap initialHeap_;
    // Allocate space at initialization or not
    AllocatingBehaviour allocating_;

    MNewArray(types::CompilerConstraintList *constraints, uint32_t count, JSObject *templateObject,
              gc::InitialHeap initialHeap, AllocatingBehaviour allocating)
      : count_(count),
        templateObject_(templateObject),
        initialHeap_(initialHeap),
        allocating_(allocating)
    {
        setResultType(MIRType_Object);
        if (!templateObject->hasSingletonType())
            setResultTypeSet(MakeSingletonTypeSet(constraints, templateObject));
    }

  public:
    INSTRUCTION_HEADER(NewArray)

    static MNewArray *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                          uint32_t count, JSObject *templateObject,
                          gc::InitialHeap initialHeap, AllocatingBehaviour allocating)
    {
        return new(alloc) MNewArray(constraints, count, templateObject, initialHeap, allocating);
    }

    uint32_t count() const {
        return count_;
    }

    JSObject *templateObject() const {
        return templateObject_;
    }

    gc::InitialHeap initialHeap() const {
        return initialHeap_;
    }

    bool isAllocating() const {
        return allocating_ == NewArray_Allocating;
    }

    // Returns true if the code generator should call through to the
    // VM rather than the fast path.
    bool shouldUseVM() const;

    // NewArray is marked as non-effectful because all our allocations are
    // either lazy when we are using "new Array(length)" or bounded by the
    // script or the stack size when we are using "new Array(...)" or "[...]"
    // notations.  So we might have to allocate the array twice if we bail
    // during the computation of the first element of the square braket
    // notation.
    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MNewObject : public MNullaryInstruction
{
    CompilerRootObject templateObject_;
    gc::InitialHeap initialHeap_;
    bool templateObjectIsClassPrototype_;

    MNewObject(types::CompilerConstraintList *constraints, JSObject *templateObject,
               gc::InitialHeap initialHeap, bool templateObjectIsClassPrototype)
      : templateObject_(templateObject),
        initialHeap_(initialHeap),
        templateObjectIsClassPrototype_(templateObjectIsClassPrototype)
    {
        JS_ASSERT_IF(templateObjectIsClassPrototype, !shouldUseVM());
        setResultType(MIRType_Object);
        if (!templateObject->hasSingletonType())
            setResultTypeSet(MakeSingletonTypeSet(constraints, templateObject));
    }

  public:
    INSTRUCTION_HEADER(NewObject)

    static MNewObject *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                           JSObject *templateObject, gc::InitialHeap initialHeap,
                           bool templateObjectIsClassPrototype)
    {
        return new(alloc) MNewObject(constraints, templateObject, initialHeap,
                                     templateObjectIsClassPrototype);
    }

    // Returns true if the code generator should call through to the
    // VM rather than the fast path.
    bool shouldUseVM() const;

    bool templateObjectIsClassPrototype() const {
        return templateObjectIsClassPrototype_;
    }

    JSObject *templateObject() const {
        return templateObject_;
    }

    gc::InitialHeap initialHeap() const {
        return initialHeap_;
    }
};

// Could be allocating either a new array or a new object.
class MNewPar : public MUnaryInstruction
{
    CompilerRootObject templateObject_;

    MNewPar(MDefinition *slice, JSObject *templateObject)
      : MUnaryInstruction(slice),
        templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewPar);

    static MNewPar *New(TempAllocator &alloc, MDefinition *slice, JSObject *templateObject) {
        return new(alloc) MNewPar(slice, templateObject);
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }

    JSObject *templateObject() const {
        return templateObject_;
    }
};

// Creates a new derived type object. At runtime, this is just a call
// to `BinaryBlock::createDerived()`. That is, the MIR itself does not
// compile to particularly optimized code. However, using a distinct
// MIR for creating derived type objects allows the compiler to
// optimize ephemeral typed objects as would be created for a
// reference like `a.b.c` -- here, the `a.b` will create an ephemeral
// derived type object that aliases the memory of `a` itself. The
// specific nature of `a.b` is revealed by using
// `MNewDerivedTypedObject` rather than `MGetProperty` or what have
// you. Moreover, the compiler knows that there are no side-effects,
// so `MNewDerivedTypedObject` instructions can be reordered or pruned
// as dead code.
class MNewDerivedTypedObject
  : public MTernaryInstruction,
    public Mix3Policy<ObjectPolicy<0>,
                      ObjectPolicy<1>,
                      IntPolicy<2> >
{
  private:
    TypeRepresentationSet set_;

    MNewDerivedTypedObject(TypeRepresentationSet set,
                           MDefinition *type,
                           MDefinition *owner,
                           MDefinition *offset)
      : MTernaryInstruction(type, owner, offset),
        set_(set)
    {
        setMovable();
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewDerivedTypedObject);

    static MNewDerivedTypedObject *New(TempAllocator &alloc, TypeRepresentationSet set,
                                       MDefinition *type, MDefinition *owner, MDefinition *offset)
    {
        return new(alloc) MNewDerivedTypedObject(set, type, owner, offset);
    }

    TypeRepresentationSet set() const {
        return set_;
    }

    MDefinition *type() const {
        return getOperand(0);
    }

    MDefinition *owner() const {
        return getOperand(1);
    }

    MDefinition *offset() const {
        return getOperand(2);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Abort parallel execution.
class MAbortPar : public MAryControlInstruction<0, 0>
{
    MAbortPar()
      : MAryControlInstruction<0, 0>()
    {
        setResultType(MIRType_Undefined);
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(AbortPar);

    static MAbortPar *New(TempAllocator &alloc) {
        return new(alloc) MAbortPar();
    }
};

// Slow path for adding a property to an object without a known base.
class MInitProp
  : public MAryInstruction<2>,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
  public:
    CompilerRootPropertyName name_;

  protected:
    MInitProp(MDefinition *obj, PropertyName *name, MDefinition *value)
      : name_(name)
    {
        setOperand(0, obj);
        setOperand(1, value);
        setResultType(MIRType_None);
    }

  public:
    INSTRUCTION_HEADER(InitProp)

    static MInitProp *New(TempAllocator &alloc, MDefinition *obj, PropertyName *name,
                          MDefinition *value)
    {
        return new(alloc) MInitProp(obj, name, value);
    }

    MDefinition *getObject() const {
        return getOperand(0);
    }
    MDefinition *getValue() const {
        return getOperand(1);
    }

    PropertyName *propertyName() const {
        return name_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MInitPropGetterSetter
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1> >
{
    CompilerRootPropertyName name_;

    MInitPropGetterSetter(MDefinition *obj, PropertyName *name, MDefinition *value)
      : MBinaryInstruction(obj, value),
        name_(name)
    { }

  public:
    INSTRUCTION_HEADER(InitPropGetterSetter)

    static MInitPropGetterSetter *New(TempAllocator &alloc, MDefinition *obj, PropertyName *name,
                                      MDefinition *value)
    {
        return new(alloc) MInitPropGetterSetter(obj, name, value);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    PropertyName *name() const {
        return name_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

class MInitElem
  : public MAryInstruction<3>,
    public Mix3Policy<ObjectPolicy<0>, BoxPolicy<1>, BoxPolicy<2> >
{
    MInitElem(MDefinition *obj, MDefinition *id, MDefinition *value)
    {
        setOperand(0, obj);
        setOperand(1, id);
        setOperand(2, value);
        setResultType(MIRType_None);
    }

  public:
    INSTRUCTION_HEADER(InitElem)

    static MInitElem *New(TempAllocator &alloc, MDefinition *obj, MDefinition *id,
                          MDefinition *value)
    {
        return new(alloc) MInitElem(obj, id, value);
    }

    MDefinition *getObject() const {
        return getOperand(0);
    }
    MDefinition *getId() const {
        return getOperand(1);
    }
    MDefinition *getValue() const {
        return getOperand(2);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MInitElemGetterSetter
  : public MTernaryInstruction,
    public Mix3Policy<ObjectPolicy<0>, BoxPolicy<1>, ObjectPolicy<2> >
{
    MInitElemGetterSetter(MDefinition *obj, MDefinition *id, MDefinition *value)
      : MTernaryInstruction(obj, id, value)
    { }

  public:
    INSTRUCTION_HEADER(InitElemGetterSetter)

    static MInitElemGetterSetter *New(TempAllocator &alloc, MDefinition *obj, MDefinition *id,
                                      MDefinition *value)
    {
        return new(alloc) MInitElemGetterSetter(obj, id, value);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *idValue() const {
        return getOperand(1);
    }
    MDefinition *value() const {
        return getOperand(2);
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

class MVariadicInstruction : public MInstruction
{
    FixedList<MUse> operands_;

  protected:
    bool init(TempAllocator &alloc, size_t length) {
        return operands_.init(alloc, length);
    }

  public:
    // Will assert if called before initialization.
    MDefinition *getOperand(size_t index) const MOZ_FINAL MOZ_OVERRIDE {
        return operands_[index].producer();
    }
    size_t numOperands() const MOZ_FINAL MOZ_OVERRIDE {
        return operands_.length();
    }
    void setOperand(size_t index, MDefinition *operand) MOZ_FINAL MOZ_OVERRIDE {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }

    MUse *getUseFor(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        return &operands_[index];
    }
};

class MCall
  : public MVariadicInstruction,
    public CallPolicy
{
  private:
    // An MCall uses the MPrepareCall, MDefinition for the function, and
    // MPassArg instructions. They are stored in the same list.
    static const size_t FunctionOperandIndex   = 0;
    static const size_t NumNonArgumentOperands = 1;

  protected:
    // True if the call is for JSOP_NEW.
    bool construct_;
    // Monomorphic cache of single target from TI, or nullptr.
    CompilerRootFunction target_;
    // Original value of argc from the bytecode.
    uint32_t numActualArgs_;

    bool needsArgCheck_;

    MCall(JSFunction *target, uint32_t numActualArgs, bool construct)
      : construct_(construct),
        target_(target),
        numActualArgs_(numActualArgs),
        needsArgCheck_(true)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Call)
    static MCall *New(TempAllocator &alloc, JSFunction *target, size_t maxArgc, size_t numActualArgs,
                      bool construct, bool isDOMCall);

    void initFunction(MDefinition *func) {
        return setOperand(FunctionOperandIndex, func);
    }

    bool needsArgCheck() const {
        return needsArgCheck_;
    }

    void disableArgCheck() {
        needsArgCheck_ = false;
    }
    MDefinition *getFunction() const {
        return getOperand(FunctionOperandIndex);
    }
    void replaceFunction(MInstruction *newfunc) {
        replaceOperand(FunctionOperandIndex, newfunc);
    }

    void addArg(size_t argnum, MDefinition *arg);

    MDefinition *getArg(uint32_t index) const {
        return getOperand(NumNonArgumentOperands + index);
    }

    static size_t IndexOfThis() {
        return NumNonArgumentOperands;
    }
    static size_t IndexOfArgument(size_t index) {
        return NumNonArgumentOperands + index + 1; // +1 to skip |this|.
    }
    static size_t IndexOfStackArg(size_t index) {
        return NumNonArgumentOperands + index;
    }

    // For TI-informed monomorphic callsites.
    JSFunction *getSingleTarget() const {
        return target_;
    }

    bool isConstructing() const {
        return construct_;
    }

    // The number of stack arguments is the max between the number of formal
    // arguments and the number of actual arguments. The number of stack
    // argument includes the |undefined| padding added in case of underflow.
    // Includes |this|.
    uint32_t numStackArgs() const {
        return numOperands() - NumNonArgumentOperands;
    }

    // Does not include |this|.
    uint32_t numActualArgs() const {
        return numActualArgs_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool possiblyCalls() const {
        return true;
    }

    virtual bool isCallDOMNative() const {
        return false;
    }

    // A method that can be called to tell the MCall to figure out whether it's
    // movable or not.  This can't be done in the constructor, because it
    // depends on the arguments to the call, and those aren't passed to the
    // constructor but are set up later via addArg.
    virtual void computeMovable() {
    }
};

class MCallDOMNative : public MCall
{
    // A helper class for MCalls for DOM natives.  Note that this is NOT
    // actually a separate MIR op from MCall, because all sorts of places use
    // isCall() to check for calls and all we really want is to overload a few
    // virtual things from MCall.
  protected:
    MCallDOMNative(JSFunction *target, uint32_t numActualArgs)
        : MCall(target, numActualArgs, false)
    {
    }

    friend MCall *MCall::New(TempAllocator &alloc, JSFunction *target, size_t maxArgc,
                             size_t numActualArgs, bool construct, bool isDOMCall);

  public:
    virtual AliasSet getAliasSet() const MOZ_OVERRIDE;

    virtual bool congruentTo(MDefinition *ins) const MOZ_OVERRIDE;

    virtual bool isCallDOMNative() const MOZ_OVERRIDE {
        return true;
    }

    virtual void computeMovable() MOZ_OVERRIDE;
};

// fun.apply(self, arguments)
class MApplyArgs
  : public MAryInstruction<3>,
    public MixPolicy<ObjectPolicy<0>, MixPolicy<IntPolicy<1>, BoxPolicy<2> > >
{
  protected:
    // Monomorphic cache of single target from TI, or nullptr.
    CompilerRootFunction target_;

    MApplyArgs(JSFunction *target, MDefinition *fun, MDefinition *argc, MDefinition *self)
      : target_(target)
    {
        setOperand(0, fun);
        setOperand(1, argc);
        setOperand(2, self);
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(ApplyArgs)
    static MApplyArgs *New(TempAllocator &alloc, JSFunction *target, MDefinition *fun,
                           MDefinition *argc, MDefinition *self);

    MDefinition *getFunction() const {
        return getOperand(0);
    }

    // For TI-informed monomorphic callsites.
    JSFunction *getSingleTarget() const {
        return target_;
    }

    MDefinition *getArgc() const {
        return getOperand(1);
    }
    MDefinition *getThis() const {
        return getOperand(2);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MBail : public MNullaryInstruction
{
  protected:
    MBail()
    {
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(Bail)

    static MBail *
    New(TempAllocator &alloc) {
        return new(alloc) MBail();
    }

};

class MAssertFloat32 : public MUnaryInstruction
{
  protected:
    bool mustBeFloat32_;

    MAssertFloat32(MDefinition *value, bool mustBeFloat32)
      : MUnaryInstruction(value), mustBeFloat32_(mustBeFloat32)
    {
    }

  public:
    INSTRUCTION_HEADER(AssertFloat32)

    static MAssertFloat32 *New(TempAllocator &alloc, MDefinition *value, bool mustBeFloat32) {
        return new(alloc) MAssertFloat32(value, mustBeFloat32);
    }

    bool canConsumeFloat32() const { return true; }

    bool mustBeFloat32() const { return mustBeFloat32_; }
};

class MGetDynamicName
  : public MAryInstruction<2>,
    public MixPolicy<ObjectPolicy<0>, StringPolicy<1> >
{
  protected:
    MGetDynamicName(MDefinition *scopeChain, MDefinition *name)
    {
        setOperand(0, scopeChain);
        setOperand(1, name);
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(GetDynamicName)

    static MGetDynamicName *
    New(TempAllocator &alloc, MDefinition *scopeChain, MDefinition *name) {
        return new(alloc) MGetDynamicName(scopeChain, name);
    }

    MDefinition *getScopeChain() const {
        return getOperand(0);
    }
    MDefinition *getName() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Bailout if the input string contains 'arguments' or 'eval'.
class MFilterArgumentsOrEval
  : public MAryInstruction<1>,
    public BoxExceptPolicy<0, MIRType_String>
{
  protected:
    MFilterArgumentsOrEval(MDefinition *string)
    {
        setOperand(0, string);
        setGuard();
        setResultType(MIRType_None);
    }

  public:
    INSTRUCTION_HEADER(FilterArgumentsOrEval)

    static MFilterArgumentsOrEval *New(TempAllocator &alloc, MDefinition *string) {
        return new(alloc) MFilterArgumentsOrEval(string);
    }

    MDefinition *getString() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MCallDirectEval
  : public MAryInstruction<3>,
    public MixPolicy<ObjectPolicy<0>,
                     MixPolicy<BoxExceptPolicy<1, MIRType_String>, BoxPolicy<2> > >
{
  protected:
    MCallDirectEval(MDefinition *scopeChain, MDefinition *string, MDefinition *thisValue,
                    jsbytecode *pc)
        : pc_(pc)
    {
        setOperand(0, scopeChain);
        setOperand(1, string);
        setOperand(2, thisValue);
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CallDirectEval)

    static MCallDirectEval *
    New(TempAllocator &alloc, MDefinition *scopeChain, MDefinition *string, MDefinition *thisValue,
        jsbytecode *pc)
    {
        return new(alloc) MCallDirectEval(scopeChain, string, thisValue, pc);
    }

    MDefinition *getScopeChain() const {
        return getOperand(0);
    }
    MDefinition *getString() const {
        return getOperand(1);
    }
    MDefinition *getThisValue() const {
        return getOperand(2);
    }

    jsbytecode  *pc() const {
        return pc_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool possiblyCalls() const {
        return true;
    }

  private:
    jsbytecode *pc_;
};

class MCompare
  : public MBinaryInstruction,
    public ComparePolicy
{
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

        // Double compared to Double
        Compare_Double,

        Compare_DoubleMaybeCoerceLHS,
        Compare_DoubleMaybeCoerceRHS,

        // Float compared to Float
        Compare_Float32,

        // String compared to String
        Compare_String,

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

        // Compare 2 values bitwise
        Compare_Value,

        // All other possible compares
        Compare_Unknown
    };

  private:
    CompareType compareType_;
    JSOp jsop_;
    bool operandMightEmulateUndefined_;
    bool operandsAreNeverNaN_;

    MCompare(MDefinition *left, MDefinition *right, JSOp jsop)
      : MBinaryInstruction(left, right),
        compareType_(Compare_Unknown),
        jsop_(jsop),
        operandMightEmulateUndefined_(true),
        operandsAreNeverNaN_(false)
    {
        setResultType(MIRType_Boolean);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Compare)
    static MCompare *New(TempAllocator &alloc, MDefinition *left, MDefinition *right, JSOp op);
    static MCompare *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right, JSOp op,
                              CompareType compareType);

    bool tryFold(bool *result);
    bool evaluateConstantOperands(bool *result);
    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    void infer(BaselineInspector *inspector, jsbytecode *pc);
    CompareType compareType() const {
        return compareType_;
    }
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
    bool isFloat32Comparison() const {
        return compareType() == Compare_Float32;
    }
    void setCompareType(CompareType type) {
        compareType_ = type;
    }
    MIRType inputType();

    JSOp jsop() const {
        return jsop_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    void markNoOperandEmulatesUndefined() {
        operandMightEmulateUndefined_ = false;
    }
    bool operandMightEmulateUndefined() const {
        return operandMightEmulateUndefined_;
    }
    bool operandsAreNeverNaN() const {
        return operandsAreNeverNaN_;
    }
    AliasSet getAliasSet() const {
        // Strict equality is never effectful.
        if (jsop_ == JSOP_STRICTEQ || jsop_ == JSOP_STRICTNE)
            return AliasSet::None();
        if (compareType_ == Compare_Unknown)
            return AliasSet::Store(AliasSet::Any);
        JS_ASSERT(compareType_ <= Compare_Value);
        return AliasSet::None();
    }

    void printOpcode(FILE *fp) const;
    void collectRangeInfoPreTrunc();

    void trySpecializeFloat32(TempAllocator &alloc);
    bool isFloat32Commutative() const { return true; }
    bool truncate();
    bool isOperandTruncated(size_t index) const;

# ifdef DEBUG
    bool isConsistentFloat32Use() const {
        return compareType_ == Compare_Float32;
    }
# endif

  protected:
    bool congruentTo(MDefinition *ins) const {
        if (!MBinaryInstruction::congruentTo(ins))
            return false;
        return compareType() == ins->toCompare()->compareType() &&
               jsop() == ins->toCompare()->jsop();
    }
};

// Takes a typed value and returns an untyped value.
class MBox : public MUnaryInstruction
{
    MBox(TempAllocator &alloc, MDefinition *ins)
      : MUnaryInstruction(ins)
    {
        setResultType(MIRType_Value);
        if (ins->resultTypeSet()) {
            setResultTypeSet(ins->resultTypeSet());
        } else if (ins->type() != MIRType_Value) {
            types::Type ntype = ins->type() == MIRType_Object
                                ? types::Type::AnyObjectType()
                                : types::Type::PrimitiveType(ValueTypeFromMIRType(ins->type()));
            setResultTypeSet(alloc.lifoAlloc()->new_<types::TemporaryTypeSet>(ntype));
        }
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Box)
    static MBox *New(TempAllocator &alloc, MDefinition *ins)
    {
        // Cannot box a box.
        JS_ASSERT(ins->type() != MIRType_Value);

        return new(alloc) MBox(alloc, ins);
    }

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Note: the op may have been inverted during lowering (to put constants in a
// position where they can be immediates), so it is important to use the
// lir->jsop() instead of the mir->jsop() when it is present.
static inline Assembler::Condition
JSOpToCondition(MCompare::CompareType compareType, JSOp op)
{
    bool isSigned = (compareType != MCompare::Compare_UInt32);
    return JSOpToCondition(op, isSigned);
}

// Takes a typed value and checks if it is a certain type. If so, the payload
// is unpacked and returned as that type. Otherwise, it is considered a
// deoptimization.
class MUnbox : public MUnaryInstruction, public BoxInputsPolicy
{
  public:
    enum Mode {
        Fallible,       // Check the type, and deoptimize if unexpected.
        Infallible,     // Type guard is not necessary.
        TypeBarrier     // Guard on the type, and act like a TypeBarrier on failure.
    };

  private:
    Mode mode_;
    BailoutKind bailoutKind_;

    MUnbox(MDefinition *ins, MIRType type, Mode mode, BailoutKind kind)
      : MUnaryInstruction(ins),
        mode_(mode)
    {
        JS_ASSERT(ins->type() == MIRType_Value);
        JS_ASSERT(type == MIRType_Boolean ||
                  type == MIRType_Int32   ||
                  type == MIRType_Double  ||
                  type == MIRType_String  ||
                  type == MIRType_Object);

        setResultType(type);
        setResultTypeSet(ins->resultTypeSet());
        setMovable();

        if (mode_ == TypeBarrier || mode_ == Fallible)
            setGuard();

        bailoutKind_ = kind;
    }
  public:
    INSTRUCTION_HEADER(Unbox)
    static MUnbox *New(TempAllocator &alloc, MDefinition *ins, MIRType type, Mode mode)
    {
        return new(alloc) MUnbox(ins, type, mode, Bailout_Normal);
    }

    static MUnbox *New(TempAllocator &alloc, MDefinition *ins, MIRType type, Mode mode,
                       BailoutKind kind)
    {
        return new(alloc) MUnbox(ins, type, mode, kind);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    Mode mode() const {
        return mode_;
    }
    BailoutKind bailoutKind() const {
        // If infallible, no bailout should be generated.
        JS_ASSERT(fallible());
        return bailoutKind_;
    }
    bool fallible() const {
        return mode() != Infallible;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isUnbox() || ins->toUnbox()->mode() != mode())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void printOpcode(FILE *fp) const;
    void makeInfallible() {
        // Should only be called if we're already Infallible or TypeBarrier
        JS_ASSERT(mode() != Fallible);
        mode_ = Infallible;
    }
};

class MGuardObject : public MUnaryInstruction, public SingleObjectPolicy
{
    MGuardObject(MDefinition *ins)
      : MUnaryInstruction(ins)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(GuardObject)

    static MGuardObject *New(TempAllocator &alloc, MDefinition *ins) {
        return new(alloc) MGuardObject(ins);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MGuardString
  : public MUnaryInstruction,
    public StringPolicy<0>
{
    MGuardString(MDefinition *ins)
      : MUnaryInstruction(ins)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_String);
    }

  public:
    INSTRUCTION_HEADER(GuardString)

    static MGuardString *New(TempAllocator &alloc, MDefinition *ins) {
        return new(alloc) MGuardString(ins);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MAssertRange
  : public MUnaryInstruction
{
    // This is the range checked by the assertion. Don't confuse this with the
    // range_ member or the range() accessor. Since MAssertRange doesn't return
    // a value, it doesn't use those.
    const Range *assertedRange_;

    MAssertRange(MDefinition *ins, const Range *assertedRange)
      : MUnaryInstruction(ins), assertedRange_(assertedRange)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_None);
    }

  public:
    INSTRUCTION_HEADER(AssertRange)

    static MAssertRange *New(TempAllocator &alloc, MDefinition *ins, const Range *assertedRange) {
        return new(alloc) MAssertRange(ins, assertedRange);
    }

    const Range *assertedRange() const {
        return assertedRange_;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void printOpcode(FILE *fp) const;
};

// Caller-side allocation of |this| for |new|:
// Given a templateobject, construct |this| for JSOP_NEW
class MCreateThisWithTemplate
  : public MNullaryInstruction
{
    // Template for |this|, provided by TI
    CompilerRootObject templateObject_;
    gc::InitialHeap initialHeap_;

    MCreateThisWithTemplate(types::CompilerConstraintList *constraints, JSObject *templateObject,
                            gc::InitialHeap initialHeap)
      : templateObject_(templateObject),
        initialHeap_(initialHeap)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(constraints, templateObject));
    }

  public:
    INSTRUCTION_HEADER(CreateThisWithTemplate);
    static MCreateThisWithTemplate *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                                        JSObject *templateObject, gc::InitialHeap initialHeap)
    {
        return new(alloc) MCreateThisWithTemplate(constraints, templateObject, initialHeap);
    }

    JSObject *templateObject() const {
        return templateObject_;
    }

    gc::InitialHeap initialHeap() const {
        return initialHeap_;
    }

    // Although creation of |this| modifies global state, it is safely repeatable.
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Caller-side allocation of |this| for |new|:
// Given a prototype operand, construct |this| for JSOP_NEW.
class MCreateThisWithProto
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1> >
{
    MCreateThisWithProto(MDefinition *callee, MDefinition *prototype)
      : MBinaryInstruction(callee, prototype)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(CreateThisWithProto)
    static MCreateThisWithProto *New(TempAllocator &alloc, MDefinition *callee,
                                     MDefinition *prototype)
    {
        return new(alloc) MCreateThisWithProto(callee, prototype);
    }

    MDefinition *getCallee() const {
        return getOperand(0);
    }
    MDefinition *getPrototype() const {
        return getOperand(1);
    }

    // Although creation of |this| modifies global state, it is safely repeatable.
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Caller-side allocation of |this| for |new|:
// Constructs |this| when possible, else MagicValue(JS_IS_CONSTRUCTING).
class MCreateThis
  : public MUnaryInstruction,
    public ObjectPolicy<0>
{
    MCreateThis(MDefinition *callee)
      : MUnaryInstruction(callee)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CreateThis)
    static MCreateThis *New(TempAllocator &alloc, MDefinition *callee)
    {
        return new(alloc) MCreateThis(callee);
    }

    MDefinition *getCallee() const {
        return getOperand(0);
    }

    // Although creation of |this| modifies global state, it is safely repeatable.
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Eager initialization of arguments object.
class MCreateArgumentsObject
  : public MUnaryInstruction,
    public ObjectPolicy<0>
{
    MCreateArgumentsObject(MDefinition *callObj)
      : MUnaryInstruction(callObj)
    {
        setResultType(MIRType_Object);
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(CreateArgumentsObject)
    static MCreateArgumentsObject *New(TempAllocator &alloc, MDefinition *callObj) {
        return new(alloc) MCreateArgumentsObject(callObj);
    }

    MDefinition *getCallObject() const {
        return getOperand(0);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MGetArgumentsObjectArg
  : public MUnaryInstruction,
    public ObjectPolicy<0>
{
    size_t argno_;

    MGetArgumentsObjectArg(MDefinition *argsObject, size_t argno)
      : MUnaryInstruction(argsObject),
        argno_(argno)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(GetArgumentsObjectArg)
    static MGetArgumentsObjectArg *New(TempAllocator &alloc, MDefinition *argsObj, size_t argno)
    {
        return new(alloc) MGetArgumentsObjectArg(argsObj, argno);
    }

    MDefinition *getArgsObject() const {
        return getOperand(0);
    }

    size_t argno() const {
        return argno_;
    }

    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::Any);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MSetArgumentsObjectArg
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
    size_t argno_;

    MSetArgumentsObjectArg(MDefinition *argsObj, size_t argno, MDefinition *value)
      : MBinaryInstruction(argsObj, value),
        argno_(argno)
    {
    }

  public:
    INSTRUCTION_HEADER(SetArgumentsObjectArg)
    static MSetArgumentsObjectArg *New(TempAllocator &alloc, MDefinition *argsObj, size_t argno,
                                       MDefinition *value)
    {
        return new(alloc) MSetArgumentsObjectArg(argsObj, argno, value);
    }

    MDefinition *getArgsObject() const {
        return getOperand(0);
    }

    size_t argno() const {
        return argno_;
    }

    MDefinition *getValue() const {
        return getOperand(1);
    }

    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Any);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MRunOncePrologue
  : public MNullaryInstruction
{
  protected:
    MRunOncePrologue()
    {
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(RunOncePrologue)

    static MRunOncePrologue *New(TempAllocator &alloc) {
        return new(alloc) MRunOncePrologue();
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Given a MIRType_Value A and a MIRType_Object B:
// If the Value may be safely unboxed to an Object, return Object(A).
// Otherwise, return B.
// Used to implement return behavior for inlined constructors.
class MReturnFromCtor
  : public MAryInstruction<2>,
    public MixPolicy<BoxPolicy<0>, ObjectPolicy<1> >
{
    MReturnFromCtor(MDefinition *value, MDefinition *object) {
        setOperand(0, value);
        setOperand(1, object);
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(ReturnFromCtor)
    static MReturnFromCtor *New(TempAllocator &alloc, MDefinition *value, MDefinition *object)
    {
        return new(alloc) MReturnFromCtor(value, object);
    }

    MDefinition *getValue() const {
        return getOperand(0);
    }
    MDefinition *getObject() const {
        return getOperand(1);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

// Converts a primitive (either typed or untyped) to a double. If the input is
// not primitive at runtime, a bailout occurs.
class MToDouble
  : public MUnaryInstruction,
    public ToDoublePolicy
{
  public:
    // Types of values which can be converted.
    enum ConversionKind {
        NonStringPrimitives,
        NonNullNonStringPrimitives,
        NumbersOnly
    };

  private:
    ConversionKind conversion_;

    MToDouble(MDefinition *def, ConversionKind conversion = NonStringPrimitives)
      : MUnaryInstruction(def), conversion_(conversion)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ToDouble)
    static MToDouble *New(TempAllocator &alloc, MDefinition *def,
                          ConversionKind conversion = NonStringPrimitives)
    {
        return new(alloc) MToDouble(def, conversion);
    }
    static MToDouble *NewAsmJS(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MToDouble(def);
    }

    ConversionKind conversion() const {
        return conversion_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isToDouble() || ins->toToDouble()->conversion() != conversion())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);
    bool truncate();
    bool isOperandTruncated(size_t index) const;

#ifdef DEBUG
    bool isConsistentFloat32Use() const { return true; }
#endif
};

// Converts a primitive (either typed or untyped) to a float32. If the input is
// not primitive at runtime, a bailout occurs.
class MToFloat32
  : public MUnaryInstruction,
    public ToDoublePolicy
{
  public:
    // Types of values which can be converted.
    enum ConversionKind {
        NonStringPrimitives,
        NonNullNonStringPrimitives,
        NumbersOnly
    };

  protected:
    ConversionKind conversion_;

    MToFloat32(MDefinition *def, ConversionKind conversion)
      : MUnaryInstruction(def), conversion_(conversion)
    {
        setResultType(MIRType_Float32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ToFloat32)
    static MToFloat32 *New(TempAllocator &alloc, MDefinition *def,
                           ConversionKind conversion = NonStringPrimitives)
    {
        return new(alloc) MToFloat32(def, conversion);
    }
    static MToFloat32 *NewAsmJS(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MToFloat32(def, NonStringPrimitives);
    }

    ConversionKind conversion() const {
        return conversion_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    virtual MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isToFloat32() || ins->toToFloat32()->conversion() != conversion())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);

    bool canConsumeFloat32() const { return true; }
    bool canProduceFloat32() const { return true; }
};

// Converts a uint32 to a double (coming from asm.js).
class MAsmJSUnsignedToDouble
  : public MUnaryInstruction
{
    MAsmJSUnsignedToDouble(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(AsmJSUnsignedToDouble);
    static MAsmJSUnsignedToDouble *NewAsmJS(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MAsmJSUnsignedToDouble(def);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Converts a uint32 to a float32 (coming from asm.js).
class MAsmJSUnsignedToFloat32
  : public MUnaryInstruction
{
    MAsmJSUnsignedToFloat32(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Float32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(AsmJSUnsignedToFloat32);
    static MAsmJSUnsignedToFloat32 *NewAsmJS(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MAsmJSUnsignedToFloat32(def);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    bool canProduceFloat32() const { return true; }
};

// Converts a primitive (either typed or untyped) to an int32. If the input is
// not primitive at runtime, a bailout occurs. If the input cannot be converted
// to an int32 without loss (i.e. "5.5" or undefined) then a bailout occurs.
class MToInt32
  : public MUnaryInstruction,
    public ToInt32Policy
{
    bool canBeNegativeZero_;
    MacroAssembler::IntConversionInputKind conversion_;

    MToInt32(MDefinition *def, MacroAssembler::IntConversionInputKind conversion)
      : MUnaryInstruction(def),
        canBeNegativeZero_(true),
        conversion_(conversion)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ToInt32)
    static MToInt32 *New(TempAllocator &alloc, MDefinition *def,
                         MacroAssembler::IntConversionInputKind conversion =
                             MacroAssembler::IntConversion_Any)
    {
        return new(alloc) MToInt32(def, conversion);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    // this only has backwards information flow.
    void analyzeEdgeCasesBackward();

    bool canBeNegativeZero() const {
        return canBeNegativeZero_;
    }
    void setCanBeNegativeZero(bool negativeZero) {
        canBeNegativeZero_ = negativeZero;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MacroAssembler::IntConversionInputKind conversion() const {
        return conversion_;
    }

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);

#ifdef DEBUG
    bool isConsistentFloat32Use() const { return true; }
#endif
};

// Converts a value or typed input to a truncated int32, for use with bitwise
// operations. This is an infallible ValueToECMAInt32.
class MTruncateToInt32 : public MUnaryInstruction
{
    MTruncateToInt32(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TruncateToInt32)
    static MTruncateToInt32 *New(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MTruncateToInt32(def);
    }
    static MTruncateToInt32 *NewAsmJS(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MTruncateToInt32(def);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);
    bool isOperandTruncated(size_t index) const;
# ifdef DEBUG
    bool isConsistentFloat32Use() const {
        return true;
    }
#endif
};

// Converts any type to a string
class MToString : public MUnaryInstruction
{
    MToString(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_String);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ToString)
    static MToString *New(TempAllocator &alloc, MDefinition *def)
    {
        return new(alloc) MToString(def);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        JS_ASSERT(input()->type() < MIRType_Object);
        return AliasSet::None();
    }
};

class MBitNot
  : public MUnaryInstruction,
    public BitwisePolicy
{
  protected:
    MBitNot(MDefinition *input)
      : MUnaryInstruction(input)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(BitNot)
    static MBitNot *New(TempAllocator &alloc, MDefinition *input);
    static MBitNot *NewAsmJS(TempAllocator &alloc, MDefinition *input);

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    void infer();

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        if (specialization_ == MIRType_None)
            return AliasSet::Store(AliasSet::Any);
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);
};

class MTypeOf
  : public MUnaryInstruction,
    public BoxInputsPolicy
{
    MIRType inputType_;
    bool inputMaybeCallableOrEmulatesUndefined_;

    MTypeOf(MDefinition *def, MIRType inputType)
      : MUnaryInstruction(def), inputType_(inputType),
        inputMaybeCallableOrEmulatesUndefined_(true)
    {
        setResultType(MIRType_String);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TypeOf)

    static MTypeOf *New(TempAllocator &alloc, MDefinition *def, MIRType inputType) {
        return new(alloc) MTypeOf(def, inputType);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MIRType inputType() const {
        return inputType_;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    void infer();

    bool inputMaybeCallableOrEmulatesUndefined() const {
        return inputMaybeCallableOrEmulatesUndefined_;
    }
    void markInputNotCallableOrEmulatesUndefined() {
        inputMaybeCallableOrEmulatesUndefined_ = false;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MToId
  : public MBinaryInstruction,
    public BoxInputsPolicy
{
    MToId(MDefinition *object, MDefinition *index)
      : MBinaryInstruction(object, index)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(ToId)

    static MToId *New(TempAllocator &alloc, MDefinition *object, MDefinition *index) {
        return new(alloc) MToId(object, index);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MBinaryBitwiseInstruction
  : public MBinaryInstruction,
    public BitwisePolicy
{
  protected:
    MBinaryBitwiseInstruction(MDefinition *left, MDefinition *right)
      : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

    void specializeForAsmJS();

  public:
    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    MDefinition *foldUnnecessaryBitop();
    virtual MDefinition *foldIfZero(size_t operand) = 0;
    virtual MDefinition *foldIfNegOne(size_t operand) = 0;
    virtual MDefinition *foldIfEqual()  = 0;
    virtual void infer(BaselineInspector *inspector, jsbytecode *pc);

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        if (specialization_ >= MIRType_Object)
            return AliasSet::Store(AliasSet::Any);
        return AliasSet::None();
    }

    bool isOperandTruncated(size_t index) const;
};

class MBitAnd : public MBinaryBitwiseInstruction
{
    MBitAnd(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitAnd)
    static MBitAnd *New(TempAllocator &alloc, MDefinition *left, MDefinition *right);
    static MBitAnd *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(operand); // 0 & x => 0;
    }
    MDefinition *foldIfNegOne(size_t operand) {
        return getOperand(1 - operand); // x & -1 => x
    }
    MDefinition *foldIfEqual() {
        return getOperand(0); // x & x => x;
    }
    void computeRange(TempAllocator &alloc);
};

class MBitOr : public MBinaryBitwiseInstruction
{
    MBitOr(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitOr)
    static MBitOr *New(TempAllocator &alloc, MDefinition *left, MDefinition *right);
    static MBitOr *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(1 - operand); // 0 | x => x, so if ith is 0, return (1-i)th
    }
    MDefinition *foldIfNegOne(size_t operand) {
        return getOperand(operand); // x | -1 => -1
    }
    MDefinition *foldIfEqual() {
        return getOperand(0); // x | x => x
    }
    void computeRange(TempAllocator &alloc);
};

class MBitXor : public MBinaryBitwiseInstruction
{
    MBitXor(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitXor)
    static MBitXor *New(TempAllocator &alloc, MDefinition *left, MDefinition *right);
    static MBitXor *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(1 - operand); // 0 ^ x => x
    }
    MDefinition *foldIfNegOne(size_t operand) {
        return this;
    }
    MDefinition *foldIfEqual() {
        return this;
    }
    void computeRange(TempAllocator &alloc);
};

class MShiftInstruction
  : public MBinaryBitwiseInstruction
{
  protected:
    MShiftInstruction(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    MDefinition *foldIfNegOne(size_t operand) {
        return this;
    }
    MDefinition *foldIfEqual() {
        return this;
    }
    virtual void infer(BaselineInspector *inspector, jsbytecode *pc);
};

class MLsh : public MShiftInstruction
{
    MLsh(MDefinition *left, MDefinition *right)
      : MShiftInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(Lsh)
    static MLsh *New(TempAllocator &alloc, MDefinition *left, MDefinition *right);
    static MLsh *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 << x => 0
        // x << 0 => x
        return getOperand(0);
    }

    void computeRange(TempAllocator &alloc);
};

class MRsh : public MShiftInstruction
{
    MRsh(MDefinition *left, MDefinition *right)
      : MShiftInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(Rsh)
    static MRsh *New(TempAllocator &alloc, MDefinition *left, MDefinition *right);
    static MRsh *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 >> x => 0
        // x >> 0 => x
        return getOperand(0);
    }
    void computeRange(TempAllocator &alloc);
};

class MUrsh : public MShiftInstruction
{
    bool bailoutsDisabled_;

    MUrsh(MDefinition *left, MDefinition *right)
      : MShiftInstruction(left, right),
        bailoutsDisabled_(false)
    { }

  public:
    INSTRUCTION_HEADER(Ursh)
    static MUrsh *New(TempAllocator &alloc, MDefinition *left, MDefinition *right);
    static MUrsh *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 >>> x => 0
        if (operand == 0)
            return getOperand(0);

        return this;
    }

    void infer(BaselineInspector *inspector, jsbytecode *pc);

    bool bailoutsDisabled() const {
        return bailoutsDisabled_;
    }

    bool fallible() const;

    void computeRange(TempAllocator &alloc);
    void collectRangeInfoPreTrunc();
};

class MBinaryArithInstruction
  : public MBinaryInstruction,
    public ArithPolicy
{
    // Implicit truncate flag is set by the truncate backward range analysis
    // optimization phase, and by asm.js pre-processing. It is used in
    // NeedNegativeZeroCheck to check if the result of a multiplication needs to
    // produce -0 double value, and for avoiding overflow checks.

    // This optimization happens when the multiplication cannot be truncated
    // even if all uses are truncating its result, such as when the range
    // analysis detect a precision loss in the multiplication.
    bool implicitTruncate_;

    void inferFallback(BaselineInspector *inspector, jsbytecode *pc);

  public:
    MBinaryArithInstruction(MDefinition *left, MDefinition *right)
      : MBinaryInstruction(left, right),
        implicitTruncate_(false)
    {
        setMovable();
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MIRType specialization() const {
        return specialization_;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    virtual double getIdentity() = 0;

    void infer(TempAllocator &alloc, BaselineInspector *inspector, jsbytecode *pc);

    void setInt32() {
        specialization_ = MIRType_Int32;
        setResultType(MIRType_Int32);
    }

    virtual void trySpecializeFloat32(TempAllocator &alloc);

    bool congruentTo(MDefinition *ins) const {
        return MBinaryInstruction::congruentTo(ins);
    }
    AliasSet getAliasSet() const {
        if (specialization_ >= MIRType_Object)
            return AliasSet::Store(AliasSet::Any);
        return AliasSet::None();
    }

    bool isTruncated() const {
        return implicitTruncate_;
    }
    void setTruncated(bool truncate) {
        implicitTruncate_ = truncate;
    }
};

class MMinMax
  : public MBinaryInstruction,
    public ArithPolicy
{
    bool isMax_;

    MMinMax(MDefinition *left, MDefinition *right, MIRType type, bool isMax)
      : MBinaryInstruction(left, right),
        isMax_(isMax)
    {
        JS_ASSERT(type == MIRType_Double || type == MIRType_Int32);
        setResultType(type);
        setMovable();
        specialization_ = type;
    }

  public:
    INSTRUCTION_HEADER(MinMax)
    static MMinMax *New(TempAllocator &alloc, MDefinition *left, MDefinition *right, MIRType type,
                        bool isMax)
    {
        return new(alloc) MMinMax(left, right, type, isMax);
    }

    bool isMax() const {
        return isMax_;
    }
    MIRType specialization() const {
        return specialization_;
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isMinMax())
            return false;
        if (isMax() != ins->toMinMax()->isMax())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);
};

class MAbs
  : public MUnaryInstruction,
    public ArithPolicy
{
    bool implicitTruncate_;

    MAbs(MDefinition *num, MIRType type)
      : MUnaryInstruction(num),
        implicitTruncate_(false)
    {
        JS_ASSERT(IsNumberType(type));
        setResultType(type);
        setMovable();
        specialization_ = type;
    }

  public:
    INSTRUCTION_HEADER(Abs)
    static MAbs *New(TempAllocator &alloc, MDefinition *num, MIRType type) {
        return new(alloc) MAbs(num, type);
    }
    static MAbs *NewAsmJS(TempAllocator &alloc, MDefinition *num, MIRType type) {
        MAbs *ins = new(alloc) MAbs(num, type);
        if (type == MIRType_Int32)
            ins->implicitTruncate_ = true;
        return ins;
    }
    MDefinition *num() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    bool fallible() const;

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);
    bool isFloat32Commutative() const { return true; }
    void trySpecializeFloat32(TempAllocator &alloc);
};

// Inline implementation of Math.sqrt().
class MSqrt
  : public MUnaryInstruction,
    public FloatingPointPolicy<0>
{
    MSqrt(MDefinition *num, MIRType type)
      : MUnaryInstruction(num)
    {
        setResultType(type);
        setPolicyType(type);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Sqrt)
    static MSqrt *New(TempAllocator &alloc, MDefinition *num) {
        return new(alloc) MSqrt(num, MIRType_Double);
    }
    static MSqrt *NewAsmJS(TempAllocator &alloc, MDefinition *num, MIRType type) {
        JS_ASSERT(IsFloatingPointType(type));
        return new(alloc) MSqrt(num, type);
    }
    MDefinition *num() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);

    bool isFloat32Commutative() const { return true; }
    void trySpecializeFloat32(TempAllocator &alloc);
};

// Inline implementation of atan2 (arctangent of y/x).
class MAtan2
  : public MBinaryInstruction,
    public MixPolicy<DoublePolicy<0>, DoublePolicy<1> >
{
    MAtan2(MDefinition *y, MDefinition *x)
      : MBinaryInstruction(y, x)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Atan2)
    static MAtan2 *New(TempAllocator &alloc, MDefinition *y, MDefinition *x) {
        return new(alloc) MAtan2(y, x);
    }

    MDefinition *y() const {
        return getOperand(0);
    }

    MDefinition *x() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    bool possiblyCalls() const {
        return true;
    }
};

// Inline implementation of Math.hypot().
class MHypot
  : public MBinaryInstruction,
    public MixPolicy<DoublePolicy<0>, DoublePolicy<1> >
{
    MHypot(MDefinition *y, MDefinition *x)
      : MBinaryInstruction(x, y)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Hypot)
    static MHypot *New(TempAllocator &alloc, MDefinition *x, MDefinition *y) {
        return new(alloc) MHypot(y, x);
    }

    MDefinition *x() const {
        return getOperand(0);
    }

    MDefinition *y() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    bool possiblyCalls() const {
        return true;
    }
};

// Inline implementation of Math.pow().
class MPow
  : public MBinaryInstruction,
    public PowPolicy
{
    MPow(MDefinition *input, MDefinition *power, MIRType powerType)
      : MBinaryInstruction(input, power),
        PowPolicy(powerType)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Pow)
    static MPow *New(TempAllocator &alloc, MDefinition *input, MDefinition *power,
                     MIRType powerType)
    {
        return new(alloc) MPow(input, power, powerType);
    }

    MDefinition *input() const {
        return lhs();
    }
    MDefinition *power() const {
        return rhs();
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Inline implementation of Math.pow(x, 0.5), which subtly differs from Math.sqrt(x).
class MPowHalf
  : public MUnaryInstruction,
    public DoublePolicy<0>
{
    bool operandIsNeverNegativeInfinity_;
    bool operandIsNeverNegativeZero_;
    bool operandIsNeverNaN_;

    MPowHalf(MDefinition *input)
      : MUnaryInstruction(input),
        operandIsNeverNegativeInfinity_(false),
        operandIsNeverNegativeZero_(false),
        operandIsNeverNaN_(false)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(PowHalf)
    static MPowHalf *New(TempAllocator &alloc, MDefinition *input) {
        return new(alloc) MPowHalf(input);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    bool operandIsNeverNegativeInfinity() const {
        return operandIsNeverNegativeInfinity_;
    }
    bool operandIsNeverNegativeZero() const {
        return operandIsNeverNegativeZero_;
    }
    bool operandIsNeverNaN() const {
        return operandIsNeverNaN_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void collectRangeInfoPreTrunc();
};

// Inline implementation of Math.random().
class MRandom : public MNullaryInstruction
{
    MRandom()
    {
        setResultType(MIRType_Double);
    }

  public:
    INSTRUCTION_HEADER(Random)
    static MRandom *New(TempAllocator &alloc) {
        return new(alloc) MRandom;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    bool possiblyCalls() const {
        return true;
    }

    void computeRange(TempAllocator &alloc);
};

class MMathFunction
  : public MUnaryInstruction,
    public FloatingPointPolicy<0>
{
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
        Sign,
        Trunc,
        Cbrt,
        Floor,
        Ceil,
        Round
    };

  private:
    Function function_;
    const MathCache *cache_;

    MMathFunction(MDefinition *input, Function function, const MathCache *cache)
      : MUnaryInstruction(input), function_(function), cache_(cache)
    {
        setResultType(MIRType_Double);
        setPolicyType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(MathFunction)

    // A nullptr cache means this function will neither access nor update the cache.
    static MMathFunction *New(TempAllocator &alloc, MDefinition *input, Function function,
                              const MathCache *cache)
    {
        return new(alloc) MMathFunction(input, function, cache);
    }
    Function function() const {
        return function_;
    }
    const MathCache *cache() const {
        return cache_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isMathFunction())
            return false;
        if (ins->toMathFunction()->function() != function())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    bool possiblyCalls() const {
        return true;
    }

    void printOpcode(FILE *fp) const;

    static const char *FunctionName(Function function);

    bool isFloat32Commutative() const {
        return function_ == Log || function_ == Sin || function_ == Cos
               || function_ == Exp || function_ == Tan || function_ == ATan
               || function_ == ASin || function_ == ACos || function_ == Floor
               || function_ == Ceil;
    }
    void trySpecializeFloat32(TempAllocator &alloc);
    void computeRange(TempAllocator &alloc);
};

class MAdd : public MBinaryArithInstruction
{
    // Is this instruction really an int at heart?
    MAdd(MDefinition *left, MDefinition *right)
      : MBinaryArithInstruction(left, right)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Add)
    static MAdd *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MAdd(left, right);
    }

    static MAdd *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right,
                          MIRType type)
    {
        MAdd *add = new(alloc) MAdd(left, right);
        add->specialization_ = type;
        add->setResultType(type);
        if (type == MIRType_Int32) {
            add->setTruncated(true);
            add->setCommutative();
        }
        return add;
    }

    bool isFloat32Commutative() const { return true; }

    double getIdentity() {
        return 0;
    }

    bool fallible() const;
    void computeRange(TempAllocator &alloc);
    bool truncate();
    bool isOperandTruncated(size_t index) const;
};

class MSub : public MBinaryArithInstruction
{
    MSub(MDefinition *left, MDefinition *right)
      : MBinaryArithInstruction(left, right)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Sub)
    static MSub *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MSub(left, right);
    }
    static MSub *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right,
                          MIRType type)
    {
        MSub *sub = new(alloc) MSub(left, right);
        sub->specialization_ = type;
        sub->setResultType(type);
        if (type == MIRType_Int32)
            sub->setTruncated(true);
        return sub;
    }

    double getIdentity() {
        return 0;
    }

    bool isFloat32Commutative() const { return true; }

    bool fallible() const;
    void computeRange(TempAllocator &alloc);
    bool truncate();
    bool isOperandTruncated(size_t index) const;
};

class MMul : public MBinaryArithInstruction
{
  public:
    enum Mode {
        Normal,
        Integer
    };

  private:
    // Annotation the result could be a negative zero
    // and we need to guard this during execution.
    bool canBeNegativeZero_;

    Mode mode_;

    MMul(MDefinition *left, MDefinition *right, MIRType type, Mode mode)
      : MBinaryArithInstruction(left, right),
        canBeNegativeZero_(true),
        mode_(mode)
    {
        if (mode == Integer) {
            // This implements the required behavior for Math.imul, which
            // can never fail and always truncates its output to int32.
            canBeNegativeZero_ = false;
            setTruncated(true);
            setCommutative();
        }
        JS_ASSERT_IF(mode != Integer, mode == Normal);

        if (type != MIRType_Value)
            specialization_ = type;
        setResultType(type);
    }

  public:
    INSTRUCTION_HEADER(Mul)
    static MMul *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MMul(left, right, MIRType_Value, MMul::Normal);
    }
    static MMul *New(TempAllocator &alloc, MDefinition *left, MDefinition *right, MIRType type,
                     Mode mode = Normal)
    {
        return new(alloc) MMul(left, right, type, mode);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    void analyzeEdgeCasesForward();
    void analyzeEdgeCasesBackward();

    double getIdentity() {
        return 1;
    }

    bool congruentTo(MDefinition *ins) const {
        if (!ins->isMul())
            return false;

        MMul *mul = ins->toMul();
        if (canBeNegativeZero_ != mul->canBeNegativeZero())
            return false;

        if (mode_ != mul->mode())
            return false;

        return MBinaryInstruction::congruentTo(ins);
    }

    bool canOverflow() const;

    bool canBeNegativeZero() const {
        return canBeNegativeZero_;
    }
    void setCanBeNegativeZero(bool negativeZero) {
        canBeNegativeZero_ = negativeZero;
    }

    bool updateForReplacement(MDefinition *ins);

    bool fallible() const {
        return canBeNegativeZero_ || canOverflow();
    }

    void setSpecialization(MIRType type) {
        specialization_ = type;
    }

    bool isFloat32Commutative() const { return true; }

    void computeRange(TempAllocator &alloc);
    bool truncate();
    bool isOperandTruncated(size_t index) const;

    Mode mode() { return mode_; }
};

class MDiv : public MBinaryArithInstruction
{
    bool canBeNegativeZero_;
    bool canBeNegativeOverflow_;
    bool canBeDivideByZero_;
    bool canBeNegativeDividend_;
    bool unsigned_;

    MDiv(MDefinition *left, MDefinition *right, MIRType type)
      : MBinaryArithInstruction(left, right),
        canBeNegativeZero_(true),
        canBeNegativeOverflow_(true),
        canBeDivideByZero_(true),
        canBeNegativeDividend_(true),
        unsigned_(false)
    {
        if (type != MIRType_Value)
            specialization_ = type;
        setResultType(type);
    }

  public:
    INSTRUCTION_HEADER(Div)
    static MDiv *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MDiv(left, right, MIRType_Value);
    }
    static MDiv *New(TempAllocator &alloc, MDefinition *left, MDefinition *right, MIRType type) {
        return new(alloc) MDiv(left, right, type);
    }
    static MDiv *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right,
                          MIRType type, bool unsignd)
    {
        MDiv *div = new(alloc) MDiv(left, right, type);
        div->unsigned_ = unsignd;
        if (type == MIRType_Int32)
            div->setTruncated(true);
        return div;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);
    void analyzeEdgeCasesForward();
    void analyzeEdgeCasesBackward();

    double getIdentity() {
        MOZ_ASSUME_UNREACHABLE("not used");
    }

    bool canBeNegativeZero() const {
        return canBeNegativeZero_;
    }
    void setCanBeNegativeZero(bool negativeZero) {
        canBeNegativeZero_ = negativeZero;
    }

    bool canBeNegativeOverflow() const {
        return canBeNegativeOverflow_;
    }

    bool canBeDivideByZero() const {
        return canBeDivideByZero_;
    }

    bool canBeNegativeDividend() const {
        return canBeNegativeDividend_;
    }

    bool isUnsigned() const {
        return unsigned_;
    }

    bool isFloat32Commutative() const { return true; }

    void computeRange(TempAllocator &alloc);
    bool fallible() const;
    bool truncate();
    void collectRangeInfoPreTrunc();
};

class MMod : public MBinaryArithInstruction
{
    bool unsigned_;
    bool canBeNegativeDividend_;

    MMod(MDefinition *left, MDefinition *right, MIRType type)
      : MBinaryArithInstruction(left, right),
        unsigned_(false),
        canBeNegativeDividend_(true)
    {
        if (type != MIRType_Value)
            specialization_ = type;
        setResultType(type);
    }

  public:
    INSTRUCTION_HEADER(Mod)
    static MMod *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MMod(left, right, MIRType_Value);
    }
    static MMod *NewAsmJS(TempAllocator &alloc, MDefinition *left, MDefinition *right,
                          MIRType type, bool unsignd)
    {
        MMod *mod = new(alloc) MMod(left, right, type);
        mod->unsigned_ = unsignd;
        if (type == MIRType_Int32)
            mod->setTruncated(true);
        return mod;
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    double getIdentity() {
        MOZ_ASSUME_UNREACHABLE("not used");
    }

    bool canBeNegativeDividend() const {
        JS_ASSERT(specialization_ == MIRType_Int32);
        return canBeNegativeDividend_;
    }
    bool canBeDivideByZero() const;
    bool canBePowerOfTwoDivisor() const;

    bool isUnsigned() const {
        return unsigned_;
    }

    bool fallible() const;

    void computeRange(TempAllocator &alloc);
    bool truncate();
    void collectRangeInfoPreTrunc();
};

class MConcat
  : public MBinaryInstruction,
    public BinaryStringPolicy
{
    MConcat(MDefinition *left, MDefinition *right)
      : MBinaryInstruction(left, right)
    {
        setMovable();
        setResultType(MIRType_String);
    }

  public:
    INSTRUCTION_HEADER(Concat)
    static MConcat *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MConcat(left, right);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MConcatPar
  : public MTernaryInstruction,
    public MixPolicy<StringPolicy<1>, StringPolicy<2> >
{
    MConcatPar(MDefinition *slice, MDefinition *left, MDefinition *right)
      : MTernaryInstruction(slice, left, right)
    {
        setMovable();
        setResultType(MIRType_String);
    }

  public:
    INSTRUCTION_HEADER(ConcatPar)

    static MConcatPar *New(TempAllocator &alloc, MDefinition *slice, MConcat *concat) {
        return new(alloc) MConcatPar(slice, concat->lhs(), concat->rhs());
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }
    MDefinition *lhs() const {
        return getOperand(1);
    }
    MDefinition *rhs() const {
        return getOperand(2);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MCharCodeAt
  : public MBinaryInstruction,
    public MixPolicy<StringPolicy<0>, IntPolicy<1> >
{
    MCharCodeAt(MDefinition *str, MDefinition *index)
        : MBinaryInstruction(str, index)
    {
        setMovable();
        setResultType(MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(CharCodeAt)

    static MCharCodeAt *New(TempAllocator &alloc, MDefinition *str, MDefinition *index) {
        return new(alloc) MCharCodeAt(str, index);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    virtual AliasSet getAliasSet() const {
        // Strings are immutable, so there is no implicit dependency.
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);
};

class MFromCharCode
  : public MUnaryInstruction,
    public IntPolicy<0>
{
    MFromCharCode(MDefinition *code)
      : MUnaryInstruction(code)
    {
        setMovable();
        setResultType(MIRType_String);
    }

  public:
    INSTRUCTION_HEADER(FromCharCode)

    static MFromCharCode *New(TempAllocator &alloc, MDefinition *code) {
        return new(alloc) MFromCharCode(code);
    }

    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MStringSplit
  : public MBinaryInstruction,
    public MixPolicy<StringPolicy<0>, StringPolicy<1> >
{
    types::TypeObject *typeObject_;

    MStringSplit(types::CompilerConstraintList *constraints, MDefinition *string, MDefinition *sep,
                 JSObject *templateObject)
      : MBinaryInstruction(string, sep),
        typeObject_(templateObject->type())
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(constraints, templateObject));
    }

  public:
    INSTRUCTION_HEADER(StringSplit)

    static MStringSplit *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                             MDefinition *string, MDefinition *sep,
                             JSObject *templateObject)
    {
        return new(alloc) MStringSplit(constraints, string, sep, templateObject);
    }
    types::TypeObject *typeObject() const {
        return typeObject_;
    }
    MDefinition *string() const {
        return getOperand(0);
    }
    MDefinition *separator() const {
        return getOperand(1);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
    virtual AliasSet getAliasSet() const {
        // Although this instruction returns a new array, we don't have to mark
        // it as store instruction, see also MNewArray.
        return AliasSet::None();
    }
};

// Returns an object to use as |this| value. See also ComputeThis and
// BoxNonStrictThis in Interpreter.h.
class MComputeThis
  : public MUnaryInstruction,
    public BoxPolicy<0>
{
    MComputeThis(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(ComputeThis)

    static MComputeThis *New(TempAllocator &alloc, MDefinition *def) {
        return new(alloc) MComputeThis(def);
    }

    MDefinition *input() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }

    // Note: don't override getAliasSet: the thisObject hook can be
    // effectful.
};

class MPhi MOZ_FINAL : public MDefinition, public InlineForwardListNode<MPhi>
{
    js::Vector<MUse, 2, IonAllocPolicy> inputs_;

    uint32_t slot_;
    bool hasBackedgeType_;
    bool triedToSpecialize_;
    bool isIterator_;
    bool canProduceFloat32_;
    bool canConsumeFloat32_;

#if DEBUG
    bool specialized_;
    uint32_t capacity_;
#endif

  protected:
    MUse *getUseFor(size_t index) {
        return &inputs_[index];
    }

  public:
    INSTRUCTION_HEADER(Phi)

    MPhi(TempAllocator &alloc, uint32_t slot, MIRType resultType)
      : inputs_(alloc),
        slot_(slot),
        hasBackedgeType_(false),
        triedToSpecialize_(false),
        isIterator_(false),
        canProduceFloat32_(false),
        canConsumeFloat32_(false)
#if DEBUG
        , specialized_(false)
        , capacity_(0)
#endif
    {
        setResultType(resultType);
    }

    static MPhi *New(TempAllocator &alloc, uint32_t slot, MIRType resultType = MIRType_Value) {
        return new(alloc) MPhi(alloc, slot, resultType);
    }

    void setOperand(size_t index, MDefinition *operand) {
        // Note: after the initial IonBuilder pass, it is OK to change phi
        // operands such that they do not include the type sets of their
        // operands. This can arise during e.g. value numbering, where
        // definitions producing the same value may have different type sets.
        JS_ASSERT(index < numOperands());
        inputs_[index].set(operand, this, index);
        operand->addUse(&inputs_[index]);
    }

    void removeOperand(size_t index);

    MDefinition *getOperand(size_t index) const {
        return inputs_[index].producer();
    }
    size_t numOperands() const {
        return inputs_.length();
    }
    uint32_t slot() const {
        return slot_;
    }
    bool hasBackedgeType() const {
        return hasBackedgeType_;
    }
    bool triedToSpecialize() const {
        return triedToSpecialize_;
    }
    void specialize(MIRType type) {
        triedToSpecialize_ = true;
        setResultType(type);
    }
    void specializeType();

    // Whether this phi's type already includes information for def.
    bool typeIncludes(MDefinition *def);

    // Add types for this phi which speculate about new inputs that may come in
    // via a loop backedge.
    void addBackedgeType(MIRType type, types::TemporaryTypeSet *typeSet);

    // Initializes the operands vector to the given capacity,
    // permitting use of addInput() instead of addInputSlow().
    bool reserveLength(size_t length);

    // Use only if capacity has been reserved by reserveLength
    void addInput(MDefinition *ins);

    // Appends a new input to the input vector. May call realloc_().
    // Prefer reserveLength() and addInput() instead, where possible.
    bool addInputSlow(MDefinition *ins, bool *ptypeChange = nullptr);

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    bool congruentTo(MDefinition *ins) const;

    bool isIterator() const {
        return isIterator_;
    }
    void setIterator() {
        isIterator_ = true;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);

    MDefinition *operandIfRedundant() {
        // If this phi is redundant (e.g., phi(a,a) or b=phi(a,this)),
        // returns the operand that it will always be equal to (a, in
        // those two cases).
        MDefinition *first = getOperand(0);
        for (size_t i = 1, e = numOperands(); i < e; i++) {
            if (getOperand(i) != first && getOperand(i) != this)
                return nullptr;
        }
        return first;
    }

    bool canProduceFloat32() const {
        return canProduceFloat32_;
    }

    void setCanProduceFloat32(bool can) {
        canProduceFloat32_ = can;
    }

    bool canConsumeFloat32() const {
        return canConsumeFloat32_;
    }

    void setCanConsumeFloat32(bool can) {
        canConsumeFloat32_ = can;
    }
};

// The goal of a Beta node is to split a def at a conditionally taken
// branch, so that uses dominated by it have a different name.
class MBeta : public MUnaryInstruction
{
  private:
    // This is the range induced by a comparison and branch in a preceding
    // block. Note that this does not reflect any range constraints from
    // the input value itself, so this value may differ from the range()
    // range after it is computed.
    const Range *comparison_;

    MBeta(MDefinition *val, const Range *comp)
        : MUnaryInstruction(val),
          comparison_(comp)
    {
        setResultType(val->type());
        setResultTypeSet(val->resultTypeSet());
    }

  public:
    INSTRUCTION_HEADER(Beta)
    void printOpcode(FILE *fp) const;
    static MBeta *New(TempAllocator &alloc, MDefinition *val, const Range *comp)
    {
        return new(alloc) MBeta(val, comp);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);
};

// MIR representation of a Value on the OSR StackFrame.
// The Value is indexed off of OsrFrameReg.
class MOsrValue : public MUnaryInstruction
{
  private:
    ptrdiff_t frameOffset_;

    MOsrValue(MOsrEntry *entry, ptrdiff_t frameOffset)
      : MUnaryInstruction(entry),
        frameOffset_(frameOffset)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(OsrValue)
    static MOsrValue *New(TempAllocator &alloc, MOsrEntry *entry, ptrdiff_t frameOffset) {
        return new(alloc) MOsrValue(entry, frameOffset);
    }

    ptrdiff_t frameOffset() const {
        return frameOffset_;
    }

    MOsrEntry *entry() {
        return getOperand(0)->toOsrEntry();
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// MIR representation of a JSObject scope chain pointer on the OSR StackFrame.
// The pointer is indexed off of OsrFrameReg.
class MOsrScopeChain : public MUnaryInstruction
{
  private:
    MOsrScopeChain(MOsrEntry *entry)
      : MUnaryInstruction(entry)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(OsrScopeChain)
    static MOsrScopeChain *New(TempAllocator &alloc, MOsrEntry *entry) {
        return new(alloc) MOsrScopeChain(entry);
    }

    MOsrEntry *entry() {
        return getOperand(0)->toOsrEntry();
    }
};

// MIR representation of a JSObject ArgumentsObject pointer on the OSR StackFrame.
// The pointer is indexed off of OsrFrameReg.
class MOsrArgumentsObject : public MUnaryInstruction
{
  private:
    MOsrArgumentsObject(MOsrEntry *entry)
      : MUnaryInstruction(entry)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(OsrArgumentsObject)
    static MOsrArgumentsObject *New(TempAllocator &alloc, MOsrEntry *entry) {
        return new(alloc) MOsrArgumentsObject(entry);
    }

    MOsrEntry *entry() {
        return getOperand(0)->toOsrEntry();
    }
};

// MIR representation of the return value on the OSR StackFrame.
// The Value is indexed off of OsrFrameReg.
class MOsrReturnValue : public MUnaryInstruction
{
  private:
    MOsrReturnValue(MOsrEntry *entry)
      : MUnaryInstruction(entry)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(OsrReturnValue)
    static MOsrReturnValue *New(TempAllocator &alloc, MOsrEntry *entry) {
        return new(alloc) MOsrReturnValue(entry);
    }

    MOsrEntry *entry() {
        return getOperand(0)->toOsrEntry();
    }
};

// Check the current frame for over-recursion past the global stack limit.
class MCheckOverRecursed : public MNullaryInstruction
{
  public:
    INSTRUCTION_HEADER(CheckOverRecursed)

    static MCheckOverRecursed *New(TempAllocator &alloc) {
        return new(alloc) MCheckOverRecursed();
    }
};

// Check the current frame for over-recursion past the global stack limit.
// Uses the per-thread recursion limit.
class MCheckOverRecursedPar : public MUnaryInstruction
{
    MCheckOverRecursedPar(MDefinition *slice)
      : MUnaryInstruction(slice)
    {
        setResultType(MIRType_None);
        setGuard();
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(CheckOverRecursedPar);

    static MCheckOverRecursedPar *New(TempAllocator &alloc, MDefinition *slice) {
        return new(alloc) MCheckOverRecursedPar(slice);
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }
};

// Check for an interrupt (or rendezvous) in parallel mode.
class MCheckInterruptPar : public MUnaryInstruction
{
    MCheckInterruptPar(MDefinition *slice)
      : MUnaryInstruction(slice)
    {
        setResultType(MIRType_None);
        setGuard();
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(CheckInterruptPar);

    static MCheckInterruptPar *New(TempAllocator &alloc, MDefinition *slice) {
        return new(alloc) MCheckInterruptPar(slice);
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }
};

// Check whether we need to fire the interrupt handler.
class MInterruptCheck : public MNullaryInstruction
{
    MInterruptCheck() {
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(InterruptCheck)

    static MInterruptCheck *New(TempAllocator &alloc) {
        return new(alloc) MInterruptCheck();
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// If not defined, set a global variable to |undefined|.
class MDefVar : public MUnaryInstruction
{
    CompilerRootPropertyName name_; // Target name to be defined.
    unsigned attrs_; // Attributes to be set.

  private:
    MDefVar(PropertyName *name, unsigned attrs, MDefinition *scopeChain)
      : MUnaryInstruction(scopeChain),
        name_(name),
        attrs_(attrs)
    {
    }

  public:
    INSTRUCTION_HEADER(DefVar)

    static MDefVar *New(TempAllocator &alloc, PropertyName *name, unsigned attrs,
                        MDefinition *scopeChain)
    {
        return new(alloc) MDefVar(name, attrs, scopeChain);
    }

    PropertyName *name() const {
        return name_;
    }
    unsigned attrs() const {
        return attrs_;
    }
    MDefinition *scopeChain() const {
        return getOperand(0);
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MDefFun : public MUnaryInstruction
{
    CompilerRootFunction fun_;

  private:
    MDefFun(JSFunction *fun, MDefinition *scopeChain)
      : MUnaryInstruction(scopeChain),
        fun_(fun)
    {}

  public:
    INSTRUCTION_HEADER(DefFun)

    static MDefFun *New(TempAllocator &alloc, JSFunction *fun, MDefinition *scopeChain) {
        return new(alloc) MDefFun(fun, scopeChain);
    }

    JSFunction *fun() const {
        return fun_;
    }
    MDefinition *scopeChain() const {
        return getOperand(0);
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MRegExp : public MNullaryInstruction
{
    CompilerRoot<RegExpObject *> source_;
    bool mustClone_;

    MRegExp(types::CompilerConstraintList *constraints, RegExpObject *source, bool mustClone)
      : source_(source),
        mustClone_(mustClone)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(constraints, source));
    }

  public:
    INSTRUCTION_HEADER(RegExp)

    static MRegExp *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                        RegExpObject *source, bool mustClone)
    {
        return new(alloc) MRegExp(constraints, source, mustClone);
    }

    bool mustClone() const {
        return mustClone_;
    }
    RegExpObject *source() const {
        return source_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MRegExpExec
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<1>, StringPolicy<0> >
{
  private:

    MRegExpExec(MDefinition *regexp, MDefinition *string)
      : MBinaryInstruction(string, regexp)
    {
        // May be object or null.
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(RegExpExec)

    static MRegExpExec *New(TempAllocator &alloc, MDefinition *regexp, MDefinition *string) {
        return new(alloc) MRegExpExec(regexp, string);
    }

    MDefinition *string() const {
        return getOperand(0);
    }

    MDefinition *regexp() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool possiblyCalls() const {
        return true;
    }
};

class MRegExpTest
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<1>, StringPolicy<0> >
{
  private:

    MRegExpTest(MDefinition *regexp, MDefinition *string)
      : MBinaryInstruction(string, regexp)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(RegExpTest)

    static MRegExpTest *New(TempAllocator &alloc, MDefinition *regexp, MDefinition *string) {
        return new(alloc) MRegExpTest(regexp, string);
    }

    MDefinition *string() const {
        return getOperand(0);
    }
    MDefinition *regexp() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool possiblyCalls() const {
        return true;
    }
};

class MRegExpReplace
  : public MTernaryInstruction,
    public Mix3Policy<StringPolicy<0>, ObjectPolicy<1>, StringPolicy<2> >
{
  private:

    MRegExpReplace(MDefinition *string, MDefinition *regexp, MDefinition *replacement)
      : MTernaryInstruction(string, regexp, replacement)
    {
        setResultType(MIRType_String);
    }

  public:
    INSTRUCTION_HEADER(RegExpReplace)

    static MRegExpReplace *New(TempAllocator &alloc, MDefinition *string, MDefinition *regexp, MDefinition *replacement) {
        return new(alloc) MRegExpReplace(string, regexp, replacement);
    }

    MDefinition *string() const {
        return getOperand(0);
    }
    MDefinition *regexp() const {
        return getOperand(1);
    }
    MDefinition *replacement() const {
        return getOperand(2);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool possiblyCalls() const {
        return true;
    }
};

struct LambdaFunctionInfo
{
    // The functions used in lambdas are the canonical original function in
    // the script, and are immutable except for delazification. Record this
    // information while still on the main thread to avoid races.
    CompilerRootFunction fun;
    uint16_t flags;
    gc::Cell *scriptOrLazyScript;
    bool singletonType;
    bool useNewTypeForClone;

    LambdaFunctionInfo(JSFunction *fun)
      : fun(fun), flags(fun->flags()),
        scriptOrLazyScript(fun->hasScript()
                           ? (gc::Cell *) fun->nonLazyScript()
                           : (gc::Cell *) fun->lazyScript()),
        singletonType(fun->hasSingletonType()),
        useNewTypeForClone(types::UseNewTypeForClone(fun))
    {}

    LambdaFunctionInfo(const LambdaFunctionInfo &info)
      : fun((JSFunction *) info.fun), flags(info.flags),
        scriptOrLazyScript(info.scriptOrLazyScript),
        singletonType(info.singletonType),
        useNewTypeForClone(info.useNewTypeForClone)
    {}
};

class MLambda
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    LambdaFunctionInfo info_;

    MLambda(types::CompilerConstraintList *constraints, MDefinition *scopeChain, JSFunction *fun)
      : MUnaryInstruction(scopeChain), info_(fun)
    {
        setResultType(MIRType_Object);
        if (!fun->hasSingletonType() && !types::UseNewTypeForClone(fun))
            setResultTypeSet(MakeSingletonTypeSet(constraints, fun));
    }

  public:
    INSTRUCTION_HEADER(Lambda)

    static MLambda *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                        MDefinition *scopeChain, JSFunction *fun)
    {
        return new(alloc) MLambda(constraints, scopeChain, fun);
    }
    MDefinition *scopeChain() const {
        return getOperand(0);
    }
    const LambdaFunctionInfo &info() const {
        return info_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

class MLambdaPar
  : public MBinaryInstruction,
    public SingleObjectPolicy
{
    LambdaFunctionInfo info_;

    MLambdaPar(MDefinition *slice, MDefinition *scopeChain, JSFunction *fun,
               types::TemporaryTypeSet *resultTypes, const LambdaFunctionInfo &info)
      : MBinaryInstruction(slice, scopeChain), info_(info)
    {
        JS_ASSERT(!info_.singletonType);
        JS_ASSERT(!info_.useNewTypeForClone);
        setResultType(MIRType_Object);
        setResultTypeSet(resultTypes);
    }

  public:
    INSTRUCTION_HEADER(LambdaPar);

    static MLambdaPar *New(TempAllocator &alloc, MDefinition *slice, MLambda *lambda) {
        return new(alloc) MLambdaPar(slice, lambda->scopeChain(), lambda->info().fun,
                                     lambda->resultTypeSet(), lambda->info());
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }

    MDefinition *scopeChain() const {
        return getOperand(1);
    }

    const LambdaFunctionInfo &info() const {
        return info_;
    }
};

// Determines the implicit |this| value for function calls.
class MImplicitThis
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MImplicitThis(MDefinition *callee)
      : MUnaryInstruction(callee)
    {
        setResultType(MIRType_Value);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ImplicitThis)

    static MImplicitThis *New(TempAllocator &alloc, MDefinition *callee) {
        return new(alloc) MImplicitThis(callee);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *callee() const {
        return getOperand(0);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Returns obj->slots.
class MSlots
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MSlots(MDefinition *object)
      : MUnaryInstruction(object)
    {
        setResultType(MIRType_Slots);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Slots)

    static MSlots *New(TempAllocator &alloc, MDefinition *object) {
        return new(alloc) MSlots(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Returns obj->elements.
class MElements
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MElements(MDefinition *object)
      : MUnaryInstruction(object)
    {
        setResultType(MIRType_Elements);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Elements)

    static MElements *New(TempAllocator &alloc, MDefinition *object) {
        return new(alloc) MElements(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// A constant value for some object's array elements or typed array elements.
class MConstantElements : public MNullaryInstruction
{
    void *value_;

  protected:
    MConstantElements(void *v)
      : value_(v)
    {
        setResultType(MIRType_Elements);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ConstantElements)
    static MConstantElements *New(TempAllocator &alloc, void *v) {
        return new(alloc) MConstantElements(v);
    }

    void *value() const {
        return value_;
    }

    void printOpcode(FILE *fp) const;

    HashNumber valueHash() const {
        return (HashNumber)(size_t) value_;
    }

    bool congruentTo(MDefinition *ins) const {
        return ins->isConstantElements() && ins->toConstantElements()->value() == value();
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Passes through an object's elements, after ensuring it is entirely doubles.
class MConvertElementsToDoubles
  : public MUnaryInstruction
{
    MConvertElementsToDoubles(MDefinition *elements)
      : MUnaryInstruction(elements)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_Elements);
    }

  public:
    INSTRUCTION_HEADER(ConvertElementsToDoubles)

    static MConvertElementsToDoubles *New(TempAllocator &alloc, MDefinition *elements) {
        return new(alloc) MConvertElementsToDoubles(elements);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
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
class MMaybeToDoubleElement
  : public MBinaryInstruction,
    public IntPolicy<1>
{
    MMaybeToDoubleElement(MDefinition *elements, MDefinition *value)
      : MBinaryInstruction(elements, value)
    {
        JS_ASSERT(elements->type() == MIRType_Elements);
        setMovable();
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(MaybeToDoubleElement)

    static MMaybeToDoubleElement *New(TempAllocator &alloc, MDefinition *elements,
                                      MDefinition *value)
    {
        return new(alloc) MMaybeToDoubleElement(elements, value);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Load the initialized length from an elements header.
class MInitializedLength
  : public MUnaryInstruction
{
    MInitializedLength(MDefinition *elements)
      : MUnaryInstruction(elements)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(InitializedLength)

    static MInitializedLength *New(TempAllocator &alloc, MDefinition *elements) {
        return new(alloc) MInitializedLength(elements);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }

    void computeRange(TempAllocator &alloc);
};

// Store to the initialized length in an elements header. Note the input is an
// *index*, one less than the desired length.
class MSetInitializedLength
  : public MAryInstruction<2>
{
    MSetInitializedLength(MDefinition *elements, MDefinition *index) {
        setOperand(0, elements);
        setOperand(1, index);
    }

  public:
    INSTRUCTION_HEADER(SetInitializedLength)

    static MSetInitializedLength *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index) {
        return new(alloc) MSetInitializedLength(elements, index);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::ObjectFields);
    }
};

// Load the array length from an elements header.
class MArrayLength
  : public MUnaryInstruction
{
    MArrayLength(MDefinition *elements)
      : MUnaryInstruction(elements)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ArrayLength)

    static MArrayLength *New(TempAllocator &alloc, MDefinition *elements) {
        return new(alloc) MArrayLength(elements);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }

    void computeRange(TempAllocator &alloc);
};

// Store to the length in an elements header. Note the input is an *index*, one
// less than the desired length.
class MSetArrayLength
  : public MAryInstruction<2>
{
    MSetArrayLength(MDefinition *elements, MDefinition *index) {
        setOperand(0, elements);
        setOperand(1, index);
    }

  public:
    INSTRUCTION_HEADER(SetArrayLength)

    static MSetArrayLength *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index) {
        return new(alloc) MSetArrayLength(elements, index);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::ObjectFields);
    }
};

// Read the length of a typed array.
class MTypedArrayLength
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MTypedArrayLength(MDefinition *obj)
      : MUnaryInstruction(obj)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TypedArrayLength)

    static MTypedArrayLength *New(TempAllocator &alloc, MDefinition *obj) {
        return new(alloc) MTypedArrayLength(obj);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // The typed array |length| property is immutable, so there is no
        // implicit dependency.
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);
};

// Load a typed array's elements vector.
class MTypedArrayElements
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MTypedArrayElements(MDefinition *object)
      : MUnaryInstruction(object)
    {
        setResultType(MIRType_Elements);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TypedArrayElements)

    static MTypedArrayElements *New(TempAllocator &alloc, MDefinition *object) {
        return new(alloc) MTypedArrayElements(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Load a binary data object's "elements", which is just its opaque
// binary data space. Eventually this should probably be
// unified with `MTypedArrayElements`.
class MTypedObjectElements
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
  private:
    MTypedObjectElements(MDefinition *object)
      : MUnaryInstruction(object)
    {
        setResultType(MIRType_Elements);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TypedObjectElements)

    static MTypedObjectElements *New(TempAllocator &alloc, MDefinition *object) {
        return new(alloc) MTypedObjectElements(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Perform !-operation
class MNot
  : public MUnaryInstruction,
    public TestPolicy
{
    bool operandMightEmulateUndefined_;
    bool operandIsNeverNaN_;

  public:
    MNot(MDefinition *input)
      : MUnaryInstruction(input),
        operandMightEmulateUndefined_(true),
        operandIsNeverNaN_(false)
    {
        setResultType(MIRType_Boolean);
        setMovable();
    }

    static MNot *New(TempAllocator &alloc, MDefinition *elements) {
        return new(alloc) MNot(elements);
    }
    static MNot *NewAsmJS(TempAllocator &alloc, MDefinition *elements) {
        MNot *ins = new(alloc) MNot(elements);
        ins->setResultType(MIRType_Int32);
        return ins;
    }

    INSTRUCTION_HEADER(Not);

    void infer();
    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    void markOperandCantEmulateUndefined() {
        operandMightEmulateUndefined_ = false;
    }
    bool operandMightEmulateUndefined() const {
        return operandMightEmulateUndefined_;
    }
    bool operandIsNeverNaN() const {
        return operandIsNeverNaN_;
    }

    MDefinition *operand() const {
        return getOperand(0);
    }

    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    TypePolicy *typePolicy() {
        return this;
    }
    void collectRangeInfoPreTrunc();

    void trySpecializeFloat32(TempAllocator &alloc);
    bool isFloat32Commutative() const { return true; }
#ifdef DEBUG
    bool isConsistentFloat32Use() const {
        return true;
    }
#endif
};

// Bailout if index + minimum < 0 or index + maximum >= length. The length used
// in a bounds check must not be negative, or the wrong result may be computed
// (unsigned comparisons may be used).
class MBoundsCheck
  : public MBinaryInstruction
{
    // Range over which to perform the bounds check, may be modified by GVN.
    int32_t minimum_;
    int32_t maximum_;

    MBoundsCheck(MDefinition *index, MDefinition *length)
      : MBinaryInstruction(index, length), minimum_(0), maximum_(0)
    {
        setGuard();
        setMovable();
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(length->type() == MIRType_Int32);

        // Returns the checked index.
        setResultType(MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(BoundsCheck)

    static MBoundsCheck *New(TempAllocator &alloc, MDefinition *index, MDefinition *length) {
        return new(alloc) MBoundsCheck(index, length);
    }
    MDefinition *index() const {
        return getOperand(0);
    }
    MDefinition *length() const {
        return getOperand(1);
    }
    int32_t minimum() const {
        return minimum_;
    }
    void setMinimum(int32_t n) {
        minimum_ = n;
    }
    int32_t maximum() const {
        return maximum_;
    }
    void setMaximum(int32_t n) {
        maximum_ = n;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isBoundsCheck())
            return false;
        MBoundsCheck *other = ins->toBoundsCheck();
        if (minimum() != other->minimum() || maximum() != other->maximum())
            return false;
        return congruentIfOperandsEqual(other);
    }
    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);
};

// Bailout if index < minimum.
class MBoundsCheckLower
  : public MUnaryInstruction
{
    int32_t minimum_;
    bool fallible_;

    MBoundsCheckLower(MDefinition *index)
      : MUnaryInstruction(index), minimum_(0), fallible_(true)
    {
        setGuard();
        setMovable();
        JS_ASSERT(index->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(BoundsCheckLower)

    static MBoundsCheckLower *New(TempAllocator &alloc, MDefinition *index) {
        return new(alloc) MBoundsCheckLower(index);
    }

    MDefinition *index() const {
        return getOperand(0);
    }
    int32_t minimum() const {
        return minimum_;
    }
    void setMinimum(int32_t n) {
        minimum_ = n;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool fallible() const {
        return fallible_;
    }
    void collectRangeInfoPreTrunc();
};

// Load a value from a dense array's element vector and does a hole check if the
// array is not known to be packed.
class MLoadElement
  : public MBinaryInstruction,
    public SingleObjectPolicy
{
    bool needsHoleCheck_;
    bool loadDoubles_;

    MLoadElement(MDefinition *elements, MDefinition *index, bool needsHoleCheck, bool loadDoubles)
      : MBinaryInstruction(elements, index),
        needsHoleCheck_(needsHoleCheck),
        loadDoubles_(loadDoubles)
    {
        setResultType(MIRType_Value);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(LoadElement)

    static MLoadElement *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index,
                             bool needsHoleCheck, bool loadDoubles) {
        return new(alloc) MLoadElement(elements, index, needsHoleCheck, loadDoubles);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    bool needsHoleCheck() const {
        return needsHoleCheck_;
    }
    bool loadDoubles() const {
        return loadDoubles_;
    }
    bool fallible() const {
        return needsHoleCheck();
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::Element);
    }
};

// Load a value from a dense array's element vector. If the index is
// out-of-bounds, or the indexed slot has a hole, undefined is returned
// instead.
class MLoadElementHole
  : public MTernaryInstruction,
    public SingleObjectPolicy
{
    bool needsNegativeIntCheck_;
    bool needsHoleCheck_;

    MLoadElementHole(MDefinition *elements, MDefinition *index, MDefinition *initLength, bool needsHoleCheck)
      : MTernaryInstruction(elements, index, initLength),
        needsNegativeIntCheck_(true),
        needsHoleCheck_(needsHoleCheck)
    {
        setResultType(MIRType_Value);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(initLength->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(LoadElementHole)

    static MLoadElementHole *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index,
                                 MDefinition *initLength, bool needsHoleCheck) {
        return new(alloc) MLoadElementHole(elements, index, initLength, needsHoleCheck);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    MDefinition *initLength() const {
        return getOperand(2);
    }
    bool needsNegativeIntCheck() const {
        return needsNegativeIntCheck_;
    }
    bool needsHoleCheck() const {
        return needsHoleCheck_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::Element);
    }
    void collectRangeInfoPreTrunc();
};

class MStoreElementCommon
{
    bool needsBarrier_;
    MIRType elementType_;
    bool racy_; // if true, exempted from normal data race req. during par. exec.

  protected:
    MStoreElementCommon()
      : needsBarrier_(false),
        elementType_(MIRType_Value),
        racy_(false)
    { }

  public:
    MIRType elementType() const {
        return elementType_;
    }
    void setElementType(MIRType elementType) {
        JS_ASSERT(elementType != MIRType_None);
        elementType_ = elementType;
    }
    bool needsBarrier() const {
        return needsBarrier_;
    }
    void setNeedsBarrier() {
        needsBarrier_ = true;
    }
    bool racy() const {
        return racy_;
    }
    void setRacy() {
        racy_ = true;
    }
};

// Store a value to a dense array slots vector.
class MStoreElement
  : public MAryInstruction<3>,
    public MStoreElementCommon,
    public MixPolicy<SingleObjectPolicy, NoFloatPolicy<2> >
{
    bool needsHoleCheck_;

    MStoreElement(MDefinition *elements, MDefinition *index, MDefinition *value, bool needsHoleCheck) {
        setOperand(0, elements);
        setOperand(1, index);
        setOperand(2, value);
        needsHoleCheck_ = needsHoleCheck;
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(StoreElement)

    static MStoreElement *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index,
                              MDefinition *value, bool needsHoleCheck) {
        return new(alloc) MStoreElement(elements, index, value, needsHoleCheck);
    }
    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    MDefinition *value() const {
        return getOperand(2);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Element);
    }
    bool needsHoleCheck() const {
        return needsHoleCheck_;
    }
    bool fallible() const {
        return needsHoleCheck();
    }
};

// Like MStoreElement, but supports indexes >= initialized length. The downside
// is that we cannot hoist the elements vector and bounds check, since this
// instruction may update the (initialized) length and reallocate the elements
// vector.
class MStoreElementHole
  : public MAryInstruction<4>,
    public MStoreElementCommon,
    public MixPolicy<SingleObjectPolicy, NoFloatPolicy<3> >
{
    MStoreElementHole(MDefinition *object, MDefinition *elements,
                      MDefinition *index, MDefinition *value) {
        setOperand(0, object);
        setOperand(1, elements);
        setOperand(2, index);
        setOperand(3, value);
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(StoreElementHole)

    static MStoreElementHole *New(TempAllocator &alloc, MDefinition *object, MDefinition *elements,
                                  MDefinition *index, MDefinition *value) {
        return new(alloc) MStoreElementHole(object, elements, index, value);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *elements() const {
        return getOperand(1);
    }
    MDefinition *index() const {
        return getOperand(2);
    }
    MDefinition *value() const {
        return getOperand(3);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        // StoreElementHole can update the initialized length, the array length
        // or reallocate obj->elements.
        return AliasSet::Store(AliasSet::Element | AliasSet::ObjectFields);
    }
};

// Array.prototype.pop or Array.prototype.shift on a dense array.
class MArrayPopShift
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
  public:
    enum Mode {
        Pop,
        Shift
    };

  private:
    Mode mode_;
    bool needsHoleCheck_;
    bool maybeUndefined_;

    MArrayPopShift(MDefinition *object, Mode mode, bool needsHoleCheck, bool maybeUndefined)
      : MUnaryInstruction(object), mode_(mode), needsHoleCheck_(needsHoleCheck),
        maybeUndefined_(maybeUndefined)
    { }

  public:
    INSTRUCTION_HEADER(ArrayPopShift)

    static MArrayPopShift *New(TempAllocator &alloc, MDefinition *object, Mode mode,
                               bool needsHoleCheck, bool maybeUndefined)
    {
        return new(alloc) MArrayPopShift(object, mode, needsHoleCheck, maybeUndefined);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    bool needsHoleCheck() const {
        return needsHoleCheck_;
    }
    bool maybeUndefined() const {
        return maybeUndefined_;
    }
    bool mode() const {
        return mode_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Element | AliasSet::ObjectFields);
    }
};

// Array.prototype.push on a dense array. Returns the new array length.
class MArrayPush
  : public MBinaryInstruction,
    public MixPolicy<SingleObjectPolicy, NoFloatPolicy<1> >
{
    MArrayPush(MDefinition *object, MDefinition *value)
      : MBinaryInstruction(object, value)
    {
        setResultType(MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(ArrayPush)

    static MArrayPush *New(TempAllocator &alloc, MDefinition *object, MDefinition *value) {
        return new(alloc) MArrayPush(object, value);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Element | AliasSet::ObjectFields);
    }
    void computeRange(TempAllocator &alloc);
};

// Array.prototype.concat on two dense arrays.
class MArrayConcat
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1> >
{
    CompilerRootObject templateObj_;
    gc::InitialHeap initialHeap_;

    MArrayConcat(types::CompilerConstraintList *constraints, MDefinition *lhs, MDefinition *rhs,
                 JSObject *templateObj, gc::InitialHeap initialHeap)
      : MBinaryInstruction(lhs, rhs),
        templateObj_(templateObj),
        initialHeap_(initialHeap)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(constraints, templateObj));
    }

  public:
    INSTRUCTION_HEADER(ArrayConcat)

    static MArrayConcat *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                             MDefinition *lhs, MDefinition *rhs,
                             JSObject *templateObj, gc::InitialHeap initialHeap)
    {
        return new(alloc) MArrayConcat(constraints, lhs, rhs, templateObj, initialHeap);
    }

    JSObject *templateObj() const {
        return templateObj_;
    }

    gc::InitialHeap initialHeap() const {
        return initialHeap_;
    }

    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Element | AliasSet::ObjectFields);
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MLoadTypedArrayElement
  : public MBinaryInstruction
{
    ScalarTypeRepresentation::Type arrayType_;

    MLoadTypedArrayElement(MDefinition *elements, MDefinition *index,
                           ScalarTypeRepresentation::Type arrayType)
      : MBinaryInstruction(elements, index), arrayType_(arrayType)
    {
        setResultType(MIRType_Value);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(arrayType >= 0 && arrayType < ScalarTypeRepresentation::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(LoadTypedArrayElement)

    static MLoadTypedArrayElement *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index,
                                       ScalarTypeRepresentation::Type arrayType)
    {
        return new(alloc) MLoadTypedArrayElement(elements, index, arrayType);
    }

    ScalarTypeRepresentation::Type arrayType() const {
        return arrayType_;
    }
    bool fallible() const {
        // Bailout if the result does not fit in an int32.
        return arrayType_ == ScalarTypeRepresentation::TYPE_UINT32 && type() == MIRType_Int32;
    }
    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::TypedArrayElement);
    }

    void printOpcode(FILE *fp) const;

    void computeRange(TempAllocator &alloc);

    bool canProduceFloat32() const { return arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT32; }
};

// Load a value from a typed array. Out-of-bounds accesses are handled using
// a VM call.
class MLoadTypedArrayElementHole
  : public MBinaryInstruction,
    public SingleObjectPolicy
{
    int arrayType_;
    bool allowDouble_;

    MLoadTypedArrayElementHole(MDefinition *object, MDefinition *index, int arrayType, bool allowDouble)
      : MBinaryInstruction(object, index), arrayType_(arrayType), allowDouble_(allowDouble)
    {
        setResultType(MIRType_Value);
        setMovable();
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(arrayType >= 0 && arrayType < ScalarTypeRepresentation::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(LoadTypedArrayElementHole)

    static MLoadTypedArrayElementHole *New(TempAllocator &alloc, MDefinition *object, MDefinition *index,
                                           int arrayType, bool allowDouble)
    {
        return new(alloc) MLoadTypedArrayElementHole(object, index, arrayType, allowDouble);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool allowDouble() const {
        return allowDouble_;
    }
    bool fallible() const {
        return arrayType_ == ScalarTypeRepresentation::TYPE_UINT32 && !allowDouble_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::TypedArrayElement);
    }
    bool canProduceFloat32() const { return arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT32; }
};

// Load a value fallibly or infallibly from a statically known typed array.
class MLoadTypedArrayElementStatic
  : public MUnaryInstruction,
    public ConvertToInt32Policy<0>
{
    MLoadTypedArrayElementStatic(TypedArrayObject *typedArray, MDefinition *ptr)
      : MUnaryInstruction(ptr), typedArray_(typedArray), fallible_(true)
    {
        int type = typedArray_->type();
        if (type == ScalarTypeRepresentation::TYPE_FLOAT32)
            setResultType(MIRType_Float32);
        else if (type == ScalarTypeRepresentation::TYPE_FLOAT64)
            setResultType(MIRType_Double);
        else
            setResultType(MIRType_Int32);
    }

    CompilerRoot<TypedArrayObject*> typedArray_;
    bool fallible_;

  public:
    INSTRUCTION_HEADER(LoadTypedArrayElementStatic);

    static MLoadTypedArrayElementStatic *New(TempAllocator &alloc, TypedArrayObject *typedArray,
                                             MDefinition *ptr)
    {
        return new(alloc) MLoadTypedArrayElementStatic(typedArray, ptr);
    }

    ArrayBufferView::ViewType viewType() const {
        return (ArrayBufferView::ViewType) typedArray_->type();
    }
    void *base() const;
    size_t length() const;

    MDefinition *ptr() const { return getOperand(0); }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::TypedArrayElement);
    }

    bool fallible() const {
        return fallible_;
    }

    void setInfallible() {
        fallible_ = false;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    void computeRange(TempAllocator &alloc);
    bool truncate();
    bool canProduceFloat32() const { return typedArray_->type() == ScalarTypeRepresentation::TYPE_FLOAT32; }
};

class MStoreTypedArrayElement
  : public MTernaryInstruction,
    public StoreTypedArrayPolicy
{
    int arrayType_;

    // See note in MStoreElementCommon.
    bool racy_;

    MStoreTypedArrayElement(MDefinition *elements, MDefinition *index, MDefinition *value,
                            int arrayType)
      : MTernaryInstruction(elements, index, value), arrayType_(arrayType), racy_(false)
    {
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(arrayType >= 0 && arrayType < ScalarTypeRepresentation::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(StoreTypedArrayElement)

    static MStoreTypedArrayElement *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index,
                                        MDefinition *value, int arrayType)
    {
        return new(alloc) MStoreTypedArrayElement(elements, index, value, arrayType);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool isByteArray() const {
        return (arrayType_ == ScalarTypeRepresentation::TYPE_INT8 ||
                arrayType_ == ScalarTypeRepresentation::TYPE_UINT8 ||
                arrayType_ == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED);
    }
    bool isFloatArray() const {
        return (arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT32 ||
                arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT64);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    MDefinition *value() const {
        return getOperand(2);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::TypedArrayElement);
    }
    bool racy() const {
        return racy_;
    }
    void setRacy() {
        racy_ = true;
    }
    bool isOperandTruncated(size_t index) const;

    bool canConsumeFloat32() const { return arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT32; }
};

class MStoreTypedArrayElementHole
  : public MAryInstruction<4>,
    public StoreTypedArrayHolePolicy
{
    int arrayType_;

    MStoreTypedArrayElementHole(MDefinition *elements, MDefinition *length, MDefinition *index,
                                MDefinition *value, int arrayType)
      : MAryInstruction<4>(), arrayType_(arrayType)
    {
        setOperand(0, elements);
        setOperand(1, length);
        setOperand(2, index);
        setOperand(3, value);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(length->type() == MIRType_Int32);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(arrayType >= 0 && arrayType < ScalarTypeRepresentation::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(StoreTypedArrayElementHole)

    static MStoreTypedArrayElementHole *New(TempAllocator &alloc, MDefinition *elements,
                                            MDefinition *length, MDefinition *index,
                                            MDefinition *value, int arrayType)
    {
        return new(alloc) MStoreTypedArrayElementHole(elements, length, index, value, arrayType);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool isByteArray() const {
        return (arrayType_ == ScalarTypeRepresentation::TYPE_INT8 ||
                arrayType_ == ScalarTypeRepresentation::TYPE_UINT8 ||
                arrayType_ == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED);
    }
    bool isFloatArray() const {
        return (arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT32 ||
                arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT64);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *length() const {
        return getOperand(1);
    }
    MDefinition *index() const {
        return getOperand(2);
    }
    MDefinition *value() const {
        return getOperand(3);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::TypedArrayElement);
    }
    bool isOperandTruncated(size_t index) const;

    bool canConsumeFloat32() const { return arrayType_ == ScalarTypeRepresentation::TYPE_FLOAT32; }
};

// Store a value infallibly to a statically known typed array.
class MStoreTypedArrayElementStatic :
    public MBinaryInstruction
  , public StoreTypedArrayElementStaticPolicy
{
    MStoreTypedArrayElementStatic(TypedArrayObject *typedArray, MDefinition *ptr, MDefinition *v)
      : MBinaryInstruction(ptr, v), typedArray_(typedArray)
    {}

    CompilerRoot<TypedArrayObject*> typedArray_;

  public:
    INSTRUCTION_HEADER(StoreTypedArrayElementStatic);

    static MStoreTypedArrayElementStatic *New(TempAllocator &alloc, TypedArrayObject *typedArray,
                                              MDefinition *ptr, MDefinition *v)
    {
        return new(alloc) MStoreTypedArrayElementStatic(typedArray, ptr, v);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    ArrayBufferView::ViewType viewType() const {
        return (ArrayBufferView::ViewType) typedArray_->type();
    }
    bool isFloatArray() const {
        return (viewType() == ArrayBufferView::TYPE_FLOAT32 ||
                viewType() == ArrayBufferView::TYPE_FLOAT64);
    }

    void *base() const;
    size_t length() const;

    MDefinition *ptr() const { return getOperand(0); }
    MDefinition *value() const { return getOperand(1); }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::TypedArrayElement);
    }
    bool isOperandTruncated(size_t index) const;

    bool canConsumeFloat32() const { return typedArray_->type() == ScalarTypeRepresentation::TYPE_FLOAT32; }
};

// Compute an "effective address", i.e., a compound computation of the form:
//   base + index * scale + displacement
class MEffectiveAddress : public MBinaryInstruction
{
    MEffectiveAddress(MDefinition *base, MDefinition *index, Scale scale, int32_t displacement)
      : MBinaryInstruction(base, index), scale_(scale), displacement_(displacement)
    {
        JS_ASSERT(base->type() == MIRType_Int32);
        JS_ASSERT(index->type() == MIRType_Int32);
        setMovable();
        setResultType(MIRType_Int32);
    }

    Scale scale_;
    int32_t displacement_;

  public:
    INSTRUCTION_HEADER(EffectiveAddress);

    static MEffectiveAddress *New(TempAllocator &alloc, MDefinition *base, MDefinition *index,
                                  Scale s, int32_t d)
    {
        return new(alloc) MEffectiveAddress(base, index, s, d);
    }
    MDefinition *base() const {
        return lhs();
    }
    MDefinition *index() const {
        return rhs();
    }
    Scale scale() const {
        return scale_;
    }
    int32_t displacement() const {
        return displacement_;
    }
};

// Clamp input to range [0, 255] for Uint8ClampedArray.
class MClampToUint8
  : public MUnaryInstruction,
    public ClampPolicy
{
    MClampToUint8(MDefinition *input)
      : MUnaryInstruction(input)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ClampToUint8)

    static MClampToUint8 *New(TempAllocator &alloc, MDefinition *input) {
        return new(alloc) MClampToUint8(input);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange(TempAllocator &alloc);
};

class MLoadFixedSlot
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    size_t slot_;

  protected:
    MLoadFixedSlot(MDefinition *obj, size_t slot)
      : MUnaryInstruction(obj), slot_(slot)
    {
        setResultType(MIRType_Value);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(LoadFixedSlot)

    static MLoadFixedSlot *New(TempAllocator &alloc, MDefinition *obj, size_t slot) {
        return new(alloc) MLoadFixedSlot(obj, slot);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    size_t slot() const {
        return slot_;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isLoadFixedSlot())
            return false;
        if (slot() != ins->toLoadFixedSlot()->slot())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::FixedSlot);
    }

    bool mightAlias(MDefinition *store);
};

class MStoreFixedSlot
  : public MBinaryInstruction,
    public MixPolicy<SingleObjectPolicy, NoFloatPolicy<1> >
{
    bool needsBarrier_;
    size_t slot_;

    MStoreFixedSlot(MDefinition *obj, MDefinition *rval, size_t slot, bool barrier)
      : MBinaryInstruction(obj, rval),
        needsBarrier_(barrier),
        slot_(slot)
    { }

  public:
    INSTRUCTION_HEADER(StoreFixedSlot)

    static MStoreFixedSlot *New(TempAllocator &alloc, MDefinition *obj, size_t slot,
                                MDefinition *rval)
    {
        return new(alloc) MStoreFixedSlot(obj, rval, slot, false);
    }
    static MStoreFixedSlot *NewBarriered(TempAllocator &alloc, MDefinition *obj, size_t slot,
                                         MDefinition *rval)
    {
        return new(alloc) MStoreFixedSlot(obj, rval, slot, true);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    size_t slot() const {
        return slot_;
    }

    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::FixedSlot);
    }
    bool needsBarrier() const {
        return needsBarrier_;
    }
    void setNeedsBarrier() {
        needsBarrier_ = true;
    }
};

typedef Vector<JSObject *, 4, IonAllocPolicy> ObjectVector;
typedef Vector<bool, 4, IonAllocPolicy> BoolVector;

class InlinePropertyTable : public TempObject
{
    struct Entry : public TempObject {
        CompilerRoot<types::TypeObject *> typeObj;
        CompilerRootFunction func;

        Entry(types::TypeObject *typeObj, JSFunction *func)
          : typeObj(typeObj), func(func)
        { }
    };

    jsbytecode *pc_;
    MResumePoint *priorResumePoint_;
    Vector<Entry *, 4, IonAllocPolicy> entries_;

  public:
    InlinePropertyTable(TempAllocator &alloc, jsbytecode *pc)
      : pc_(pc), priorResumePoint_(nullptr), entries_(alloc)
    { }

    void setPriorResumePoint(MResumePoint *resumePoint) {
        JS_ASSERT(priorResumePoint_ == nullptr);
        priorResumePoint_ = resumePoint;
    }

    MResumePoint *priorResumePoint() const {
        return priorResumePoint_;
    }

    jsbytecode *pc() const {
        return pc_;
    }

    bool addEntry(TempAllocator &alloc, types::TypeObject *typeObj, JSFunction *func) {
        return entries_.append(new(alloc) Entry(typeObj, func));
    }

    size_t numEntries() const {
        return entries_.length();
    }

    types::TypeObject *getTypeObject(size_t i) const {
        JS_ASSERT(i < numEntries());
        return entries_[i]->typeObj;
    }

    JSFunction *getFunction(size_t i) const {
        JS_ASSERT(i < numEntries());
        return entries_[i]->func;
    }

    bool hasFunction(JSFunction *func) const;
    types::TemporaryTypeSet *buildTypeSetForFunction(JSFunction *func) const;

    // Remove targets that vetoed inlining from the InlinePropertyTable.
    void trimTo(ObjectVector &targets, BoolVector &choiceSet);

    // Ensure that the InlinePropertyTable's domain is a subset of |targets|.
    void trimToTargets(ObjectVector &targets);
};

class CacheLocationList : public InlineConcatList<CacheLocationList>
{
  public:
    CacheLocationList()
      : pc(nullptr),
        script(nullptr)
    { }

    jsbytecode *pc;
    JSScript *script;
};

class MGetPropertyCache
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRootPropertyName name_;
    bool idempotent_;
    bool allowGetters_;
    bool monitoredResult_;

    CacheLocationList location_;

    InlinePropertyTable *inlinePropertyTable_;

    MGetPropertyCache(MDefinition *obj, PropertyName *name, bool monitoredResult)
      : MUnaryInstruction(obj),
        name_(name),
        idempotent_(false),
        allowGetters_(false),
        monitoredResult_(monitoredResult),
        location_(),
        inlinePropertyTable_(nullptr)
    {
        setResultType(MIRType_Value);

        // The cache will invalidate if there are objects with e.g. lookup or
        // resolve hooks on the proto chain. setGuard ensures this check is not
        // eliminated.
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(GetPropertyCache)

    static MGetPropertyCache *New(TempAllocator &alloc, MDefinition *obj, PropertyName *name,
                                  bool monitoredResult) {
        return new(alloc) MGetPropertyCache(obj, name, monitoredResult);
    }

    InlinePropertyTable *initInlinePropertyTable(TempAllocator &alloc, jsbytecode *pc) {
        JS_ASSERT(inlinePropertyTable_ == nullptr);
        inlinePropertyTable_ = new(alloc) InlinePropertyTable(alloc, pc);
        return inlinePropertyTable_;
    }

    void clearInlinePropertyTable() {
        inlinePropertyTable_ = nullptr;
    }

    InlinePropertyTable *propTable() const {
        return inlinePropertyTable_;
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    PropertyName *name() const {
        return name_;
    }
    bool idempotent() const {
        return idempotent_;
    }
    void setIdempotent() {
        idempotent_ = true;
        setMovable();
    }
    bool allowGetters() const {
        return allowGetters_;
    }
    bool monitoredResult() const {
        return monitoredResult_;
    }
    void setAllowGetters() {
        allowGetters_ = true;
    }
    CacheLocationList &location() {
        return location_;
    }
    TypePolicy *typePolicy() { return this; }

    bool congruentTo(MDefinition *ins) const {
        if (!idempotent_)
            return false;
        if (!ins->isGetPropertyCache())
            return false;
        if (name() != ins->toGetPropertyCache()->name())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        if (idempotent_) {
            return AliasSet::Load(AliasSet::ObjectFields |
                                  AliasSet::FixedSlot |
                                  AliasSet::DynamicSlot);
        }
        return AliasSet::Store(AliasSet::Any);
    }

    void setBlock(MBasicBlock *block);
    bool updateForReplacement(MDefinition *ins);
};

// Emit code to load a value from an object's slots if its shape matches
// one of the shapes observed by the baseline IC, else bails out.
class MGetPropertyPolymorphic
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    struct Entry {
        // The shape to guard against.
        Shape *objShape;

        // The property to laod.
        Shape *shape;
    };

    Vector<Entry, 4, IonAllocPolicy> shapes_;
    CompilerRootPropertyName name_;

    MGetPropertyPolymorphic(TempAllocator &alloc, MDefinition *obj, PropertyName *name)
      : MUnaryInstruction(obj),
        shapes_(alloc),
        name_(name)
    {
        setMovable();
        setResultType(MIRType_Value);
    }

    PropertyName *name() const {
        return name_;
    }

  public:
    INSTRUCTION_HEADER(GetPropertyPolymorphic)

    static MGetPropertyPolymorphic *New(TempAllocator &alloc, MDefinition *obj, PropertyName *name) {
        return new(alloc) MGetPropertyPolymorphic(alloc, obj, name);
    }

    bool congruentTo(MDefinition *ins) const {
        if (!ins->isGetPropertyPolymorphic())
            return false;
        if (name() != ins->toGetPropertyPolymorphic()->name())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool addShape(Shape *objShape, Shape *shape) {
        Entry entry;
        entry.objShape = objShape;
        entry.shape = shape;
        return shapes_.append(entry);
    }
    size_t numShapes() const {
        return shapes_.length();
    }
    Shape *objShape(size_t i) const {
        return shapes_[i].objShape;
    }
    Shape *shape(size_t i) const {
        return shapes_[i].shape;
    }
    MDefinition *obj() const {
        return getOperand(0);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields | AliasSet::FixedSlot | AliasSet::DynamicSlot);
    }

    bool mightAlias(MDefinition *store);
};

// Emit code to store a value to an object's slots if its shape matches
// one of the shapes observed by the baseline IC, else bails out.
class MSetPropertyPolymorphic
  : public MBinaryInstruction,
    public SingleObjectPolicy
{
    struct Entry {
        // The shape to guard against.
        Shape *objShape;

        // The property to laod.
        Shape *shape;
    };

    Vector<Entry, 4, IonAllocPolicy> shapes_;
    bool needsBarrier_;

    MSetPropertyPolymorphic(TempAllocator &alloc, MDefinition *obj, MDefinition *value)
      : MBinaryInstruction(obj, value),
        shapes_(alloc),
        needsBarrier_(false)
    {
    }

  public:
    INSTRUCTION_HEADER(SetPropertyPolymorphic)

    static MSetPropertyPolymorphic *New(TempAllocator &alloc, MDefinition *obj, MDefinition *value) {
        return new(alloc) MSetPropertyPolymorphic(alloc, obj, value);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool addShape(Shape *objShape, Shape *shape) {
        Entry entry;
        entry.objShape = objShape;
        entry.shape = shape;
        return shapes_.append(entry);
    }
    size_t numShapes() const {
        return shapes_.length();
    }
    Shape *objShape(size_t i) const {
        return shapes_[i].objShape;
    }
    Shape *shape(size_t i) const {
        return shapes_[i].shape;
    }
    MDefinition *obj() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    bool needsBarrier() const {
        return needsBarrier_;
    }
    void setNeedsBarrier() {
        needsBarrier_ = true;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::ObjectFields | AliasSet::FixedSlot | AliasSet::DynamicSlot);
    }
};

class MDispatchInstruction
  : public MControlInstruction,
    public SingleObjectPolicy
{
    // Map from JSFunction* -> MBasicBlock.
    struct Entry {
        JSFunction *func;
        MBasicBlock *block;

        Entry(JSFunction *func, MBasicBlock *block)
          : func(func), block(block)
        { }
    };
    Vector<Entry, 4, IonAllocPolicy> map_;

    // An optional fallback path that uses MCall.
    MBasicBlock *fallback_;
    MUse operand_;

  public:
    MDispatchInstruction(TempAllocator &alloc, MDefinition *input)
      : map_(alloc), fallback_(nullptr)
    {
        setOperand(0, input);
    }

  protected:
    void setOperand(size_t index, MDefinition *operand) MOZ_FINAL MOZ_OVERRIDE {
        JS_ASSERT(index == 0);
        operand_.set(operand, this, 0);
        operand->addUse(&operand_);
    }
    MUse *getUseFor(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        JS_ASSERT(index == 0);
        return &operand_;
    }
    MDefinition *getOperand(size_t index) const MOZ_FINAL MOZ_OVERRIDE {
        JS_ASSERT(index == 0);
        return operand_.producer();
    }
    size_t numOperands() const MOZ_FINAL MOZ_OVERRIDE {
        return 1;
    }

  public:
    void setSuccessor(size_t i, MBasicBlock *successor) {
        JS_ASSERT(i < numSuccessors());
        if (i == map_.length())
            fallback_ = successor;
        else
            map_[i].block = successor;
    }
    size_t numSuccessors() const MOZ_FINAL MOZ_OVERRIDE {
        return map_.length() + (fallback_ ? 1 : 0);
    }
    void replaceSuccessor(size_t i, MBasicBlock *successor) MOZ_FINAL MOZ_OVERRIDE {
        setSuccessor(i, successor);
    }
    MBasicBlock *getSuccessor(size_t i) const MOZ_FINAL MOZ_OVERRIDE {
        JS_ASSERT(i < numSuccessors());
        if (i == map_.length())
            return fallback_;
        return map_[i].block;
    }

  public:
    void addCase(JSFunction *func, MBasicBlock *block) {
        map_.append(Entry(func, block));
    }
    uint32_t numCases() const {
        return map_.length();
    }
    JSFunction *getCase(uint32_t i) const {
        return map_[i].func;
    }
    MBasicBlock *getCaseBlock(uint32_t i) const {
        return map_[i].block;
    }

    bool hasFallback() const {
        return bool(fallback_);
    }
    void addFallback(MBasicBlock *block) {
        JS_ASSERT(!hasFallback());
        fallback_ = block;
    }
    MBasicBlock *getFallback() const {
        JS_ASSERT(hasFallback());
        return fallback_;
    }

  public:
    MDefinition *input() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

// Polymorphic dispatch for inlining, keyed off incoming TypeObject.
class MTypeObjectDispatch : public MDispatchInstruction
{
    // Map TypeObject (of CallProp's Target Object) -> JSFunction (yielded by the CallProp).
    InlinePropertyTable *inlinePropertyTable_;

    MTypeObjectDispatch(TempAllocator &alloc, MDefinition *input, InlinePropertyTable *table)
      : MDispatchInstruction(alloc, input),
        inlinePropertyTable_(table)
    { }

  public:
    INSTRUCTION_HEADER(TypeObjectDispatch)

    static MTypeObjectDispatch *New(TempAllocator &alloc, MDefinition *ins,
                                    InlinePropertyTable *table)
    {
        return new(alloc) MTypeObjectDispatch(alloc, ins, table);
    }

    InlinePropertyTable *propTable() const {
        return inlinePropertyTable_;
    }
};

// Polymorphic dispatch for inlining, keyed off incoming JSFunction*.
class MFunctionDispatch : public MDispatchInstruction
{
    MFunctionDispatch(TempAllocator &alloc, MDefinition *input)
      : MDispatchInstruction(alloc, input)
    { }

  public:
    INSTRUCTION_HEADER(FunctionDispatch)

    static MFunctionDispatch *New(TempAllocator &alloc, MDefinition *ins) {
        return new(alloc) MFunctionDispatch(alloc, ins);
    }
};

class MGetElementCache
  : public MBinaryInstruction
{
    MixPolicy<ObjectPolicy<0>, BoxPolicy<1> > PolicyV;
    MixPolicy<ObjectPolicy<0>, IntPolicy<1> > PolicyT;

    // See the comment in IonBuilder::jsop_getelem.
    bool monitoredResult_;

    MGetElementCache(MDefinition *obj, MDefinition *value, bool monitoredResult)
      : MBinaryInstruction(obj, value), monitoredResult_(monitoredResult)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(GetElementCache)

    static MGetElementCache *New(TempAllocator &alloc, MDefinition *obj, MDefinition *value,
                                 bool monitoredResult)
    {
        return new(alloc) MGetElementCache(obj, value, monitoredResult);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    bool monitoredResult() const {
        return monitoredResult_;
    }

    bool allowDoubleResult() const;

    TypePolicy *typePolicy() {
        if (type() == MIRType_Value)
            return &PolicyV;
        return &PolicyT;
    }
};

class MBindNameCache
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRootPropertyName name_;
    CompilerRootScript script_;
    jsbytecode *pc_;

    MBindNameCache(MDefinition *scopeChain, PropertyName *name, JSScript *script, jsbytecode *pc)
      : MUnaryInstruction(scopeChain), name_(name), script_(script), pc_(pc)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(BindNameCache)

    static MBindNameCache *New(TempAllocator &alloc, MDefinition *scopeChain, PropertyName *name,
                               JSScript *script, jsbytecode *pc)
    {
        return new(alloc) MBindNameCache(scopeChain, name, script, pc);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *scopeChain() const {
        return getOperand(0);
    }
    PropertyName *name() const {
        return name_;
    }
    JSScript *script() const {
        return script_;
    }
    jsbytecode *pc() const {
        return pc_;
    }
};

// Guard on an object's shape.
class MGuardShape
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRootShape shape_;
    BailoutKind bailoutKind_;

    MGuardShape(MDefinition *obj, Shape *shape, BailoutKind bailoutKind)
      : MUnaryInstruction(obj),
        shape_(shape),
        bailoutKind_(bailoutKind)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(GuardShape)

    static MGuardShape *New(TempAllocator &alloc, MDefinition *obj, Shape *shape,
                            BailoutKind bailoutKind)
    {
        return new(alloc) MGuardShape(obj, shape, bailoutKind);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *obj() const {
        return getOperand(0);
    }
    const Shape *shape() const {
        return shape_;
    }
    BailoutKind bailoutKind() const {
        return bailoutKind_;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isGuardShape())
            return false;
        if (shape() != ins->toGuardShape()->shape())
            return false;
        if (bailoutKind() != ins->toGuardShape()->bailoutKind())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Guard on an object's type, inclusively or exclusively.
class MGuardObjectType
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRoot<types::TypeObject *> typeObject_;
    bool bailOnEquality_;

    MGuardObjectType(MDefinition *obj, types::TypeObject *typeObject, bool bailOnEquality)
      : MUnaryInstruction(obj),
        typeObject_(typeObject),
        bailOnEquality_(bailOnEquality)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(GuardObjectType)

    static MGuardObjectType *New(TempAllocator &alloc, MDefinition *obj, types::TypeObject *typeObject,
                                 bool bailOnEquality) {
        return new(alloc) MGuardObjectType(obj, typeObject, bailOnEquality);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *obj() const {
        return getOperand(0);
    }
    const types::TypeObject *typeObject() const {
        return typeObject_;
    }
    bool bailOnEquality() const {
        return bailOnEquality_;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isGuardObjectType())
            return false;
        if (typeObject() != ins->toGuardObjectType()->typeObject())
            return false;
        if (bailOnEquality() != ins->toGuardObjectType()->bailOnEquality())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Guard on an object's identity, inclusively or exclusively.
class MGuardObjectIdentity
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRoot<JSObject *> singleObject_;
    bool bailOnEquality_;

    MGuardObjectIdentity(MDefinition *obj, JSObject *singleObject, bool bailOnEquality)
      : MUnaryInstruction(obj),
        singleObject_(singleObject),
        bailOnEquality_(bailOnEquality)
    {
        setGuard();
        setMovable();
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(GuardObjectIdentity)

    static MGuardObjectIdentity *New(TempAllocator &alloc, MDefinition *obj, JSObject *singleObject,
                                     bool bailOnEquality) {
        return new(alloc) MGuardObjectIdentity(obj, singleObject, bailOnEquality);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *obj() const {
        return getOperand(0);
    }
    JSObject *singleObject() const {
        return singleObject_;
    }
    bool bailOnEquality() const {
        return bailOnEquality_;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isGuardObjectIdentity())
            return false;
        if (singleObject() != ins->toGuardObjectIdentity()->singleObject())
            return false;
        if (bailOnEquality() != ins->toGuardObjectIdentity()->bailOnEquality())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Guard on an object's class.
class MGuardClass
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    const Class *class_;

    MGuardClass(MDefinition *obj, const Class *clasp)
      : MUnaryInstruction(obj),
        class_(clasp)
    {
        setGuard();
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(GuardClass)

    static MGuardClass *New(TempAllocator &alloc, MDefinition *obj, const Class *clasp) {
        return new(alloc) MGuardClass(obj, clasp);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *obj() const {
        return getOperand(0);
    }
    const Class *getClass() const {
        return class_;
    }
    bool congruentTo(MDefinition *ins) const {
        if (!ins->isGuardClass())
            return false;
        if (getClass() != ins->toGuardClass()->getClass())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Load from vp[slot] (slots that are not inline in an object).
class MLoadSlot
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    uint32_t slot_;

    MLoadSlot(MDefinition *slots, uint32_t slot)
      : MUnaryInstruction(slots),
        slot_(slot)
    {
        setResultType(MIRType_Value);
        setMovable();
        JS_ASSERT(slots->type() == MIRType_Slots);
    }

  public:
    INSTRUCTION_HEADER(LoadSlot)

    static MLoadSlot *New(TempAllocator &alloc, MDefinition *slots, uint32_t slot) {
        return new(alloc) MLoadSlot(slots, slot);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *slots() const {
        return getOperand(0);
    }
    uint32_t slot() const {
        return slot_;
    }

    bool congruentTo(MDefinition *ins) const {
        if (!ins->isLoadSlot())
            return false;
        if (slot() != ins->toLoadSlot()->slot())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        JS_ASSERT(slots()->type() == MIRType_Slots);
        return AliasSet::Load(AliasSet::DynamicSlot);
    }
    bool mightAlias(MDefinition *store);
};

// Inline call to access a function's environment (scope chain).
class MFunctionEnvironment
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
  public:
    MFunctionEnvironment(MDefinition *function)
        : MUnaryInstruction(function)
    {
        setResultType(MIRType_Object);
        setMovable();
    }

    INSTRUCTION_HEADER(FunctionEnvironment)

    static MFunctionEnvironment *New(TempAllocator &alloc, MDefinition *function) {
        return new(alloc) MFunctionEnvironment(function);
    }

    MDefinition *function() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    // A function's environment is fixed.
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Loads the current js::ForkJoinSlice*.
// Only applicable in ParallelExecution.
class MForkJoinSlice
  : public MNullaryInstruction
{
    MForkJoinSlice()
        : MNullaryInstruction()
    {
        setResultType(MIRType_ForkJoinSlice);
    }

  public:
    INSTRUCTION_HEADER(ForkJoinSlice);

    static MForkJoinSlice *New(TempAllocator &alloc) {
        return new(alloc) MForkJoinSlice();
    }

    AliasSet getAliasSet() const {
        // Indicate that this instruction reads nothing, stores nothing.
        // (For all intents and purposes)
        return AliasSet::None();
    }

    bool possiblyCalls() const {
        return true;
    }
};

// Store to vp[slot] (slots that are not inline in an object).
class MStoreSlot
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, NoFloatPolicy<1> >
{
    uint32_t slot_;
    MIRType slotType_;
    bool needsBarrier_;

    MStoreSlot(MDefinition *slots, uint32_t slot, MDefinition *value, bool barrier)
        : MBinaryInstruction(slots, value),
          slot_(slot),
          slotType_(MIRType_Value),
          needsBarrier_(barrier)
    {
        JS_ASSERT(slots->type() == MIRType_Slots);
    }

  public:
    INSTRUCTION_HEADER(StoreSlot)

    static MStoreSlot *New(TempAllocator &alloc, MDefinition *slots, uint32_t slot,
                           MDefinition *value)
    {
        return new(alloc) MStoreSlot(slots, slot, value, false);
    }
    static MStoreSlot *NewBarriered(TempAllocator &alloc, MDefinition *slots, uint32_t slot,
                                    MDefinition *value)
    {
        return new(alloc) MStoreSlot(slots, slot, value, true);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *slots() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    uint32_t slot() const {
        return slot_;
    }
    MIRType slotType() const {
        return slotType_;
    }
    void setSlotType(MIRType slotType) {
        JS_ASSERT(slotType != MIRType_None);
        slotType_ = slotType;
    }
    bool needsBarrier() const {
        return needsBarrier_;
    }
    void setNeedsBarrier() {
        needsBarrier_ = true;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::DynamicSlot);
    }
};

class MGetNameCache
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
  public:
    enum AccessKind {
        NAMETYPEOF,
        NAME
    };

  private:
    CompilerRootPropertyName name_;
    AccessKind kind_;

    MGetNameCache(MDefinition *obj, PropertyName *name, AccessKind kind)
      : MUnaryInstruction(obj),
        name_(name),
        kind_(kind)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(GetNameCache)

    static MGetNameCache *New(TempAllocator &alloc, MDefinition *obj, PropertyName *name,
                              AccessKind kind)
    {
        return new(alloc) MGetNameCache(obj, name, kind);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *scopeObj() const {
        return getOperand(0);
    }
    PropertyName *name() const {
        return name_;
    }
    AccessKind accessKind() const {
        return kind_;
    }
};

class MCallGetIntrinsicValue : public MNullaryInstruction
{
    CompilerRootPropertyName name_;

    MCallGetIntrinsicValue(PropertyName *name)
      : name_(name)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CallGetIntrinsicValue)

    static MCallGetIntrinsicValue *New(TempAllocator &alloc, PropertyName *name) {
        return new(alloc) MCallGetIntrinsicValue(name);
    }
    PropertyName *name() const {
        return name_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MCallsiteCloneCache
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    jsbytecode *callPc_;

    MCallsiteCloneCache(MDefinition *callee, jsbytecode *callPc)
      : MUnaryInstruction(callee),
        callPc_(callPc)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(CallsiteCloneCache);

    static MCallsiteCloneCache *New(TempAllocator &alloc, MDefinition *callee, jsbytecode *callPc) {
        return new(alloc) MCallsiteCloneCache(callee, callPc);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *callee() const {
        return getOperand(0);
    }
    jsbytecode *callPc() const {
        return callPc_;
    }

    // Callsite cloning is idempotent.
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MSetPropertyInstruction : public MBinaryInstruction
{
    CompilerRootPropertyName name_;
    bool strict_;
    bool needsBarrier_;

  protected:
    MSetPropertyInstruction(MDefinition *obj, MDefinition *value, PropertyName *name,
                            bool strict)
      : MBinaryInstruction(obj, value),
        name_(name), strict_(strict), needsBarrier_(true)
    {}

  public:
    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *value() const {
        return getOperand(1);
    }
    PropertyName *name() const {
        return name_;
    }
    bool strict() const {
        return strict_;
    }
    bool needsBarrier() const {
        return needsBarrier_;
    }
    void setNeedsBarrier() {
        needsBarrier_ = true;
    }
};

class MSetElementInstruction
  : public MTernaryInstruction
{
  protected:
    MSetElementInstruction(MDefinition *object, MDefinition *index, MDefinition *value)
      : MTernaryInstruction(object, index, value)
    {
    }

  public:
    MDefinition *object() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    MDefinition *value() const {
        return getOperand(2);
    }
};

class MDeleteProperty
  : public MUnaryInstruction,
    public BoxInputsPolicy
{
    CompilerRootPropertyName name_;

  protected:
    MDeleteProperty(MDefinition *val, PropertyName *name)
      : MUnaryInstruction(val),
        name_(name)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(DeleteProperty)

    static MDeleteProperty *New(TempAllocator &alloc, MDefinition *obj, PropertyName *name) {
        return new(alloc) MDeleteProperty(obj, name);
    }
    MDefinition *value() const {
        return getOperand(0);
    }
    PropertyName *name() const {
        return name_;
    }
    virtual TypePolicy *typePolicy() {
        return this;
    }
};

class MDeleteElement
  : public MBinaryInstruction,
    public BoxInputsPolicy
{
    MDeleteElement(MDefinition *value, MDefinition *index)
      : MBinaryInstruction(value, index)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(DeleteElement)

    static MDeleteElement *New(TempAllocator &alloc, MDefinition *value, MDefinition *index) {
        return new(alloc) MDeleteElement(value, index);
    }
    MDefinition *value() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    virtual TypePolicy *typePolicy() {
        return this;
    }
};

// Note: This uses CallSetElementPolicy to always box its second input,
// ensuring we don't need two LIR instructions to lower this.
class MCallSetProperty
  : public MSetPropertyInstruction,
    public CallSetElementPolicy
{
    MCallSetProperty(MDefinition *obj, MDefinition *value, PropertyName *name, bool strict)
      : MSetPropertyInstruction(obj, value, name, strict)
    {
    }

  public:
    INSTRUCTION_HEADER(CallSetProperty)

    static MCallSetProperty *New(TempAllocator &alloc, MDefinition *obj, MDefinition *value,
                                 PropertyName *name, bool strict)
    {
        return new(alloc) MCallSetProperty(obj, value, name, strict);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MSetPropertyCache
  : public MSetPropertyInstruction,
    public MixPolicy<SingleObjectPolicy, NoFloatPolicy<1> >
{
    bool needsTypeBarrier_;

    MSetPropertyCache(MDefinition *obj, MDefinition *value, PropertyName *name, bool strict,
                      bool typeBarrier)
      : MSetPropertyInstruction(obj, value, name, strict),
        needsTypeBarrier_(typeBarrier)
    {
    }

  public:
    INSTRUCTION_HEADER(SetPropertyCache)

    static MSetPropertyCache *New(TempAllocator &alloc, MDefinition *obj, MDefinition *value,
                                  PropertyName *name, bool strict, bool typeBarrier)
    {
        return new(alloc) MSetPropertyCache(obj, value, name, strict, typeBarrier);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool needsTypeBarrier() const {
        return needsTypeBarrier_;
    }
};

class MSetElementCache
  : public MSetElementInstruction,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
    bool strict_;
    bool guardHoles_;

    MSetElementCache(MDefinition *obj, MDefinition *index, MDefinition *value, bool strict,
                     bool guardHoles)
      : MSetElementInstruction(obj, index, value),
        strict_(strict),
        guardHoles_(guardHoles)
    {
    }

  public:
    INSTRUCTION_HEADER(SetElementCache);

    static MSetElementCache *New(TempAllocator &alloc, MDefinition *obj, MDefinition *index,
                                 MDefinition *value, bool strict, bool guardHoles)
    {
        return new(alloc) MSetElementCache(obj, index, value, strict, guardHoles);
    }

    bool strict() const {
        return strict_;
    }
    bool guardHoles() const {
        return guardHoles_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool canConsumeFloat32() const { return true; }
};

class MCallGetProperty
  : public MUnaryInstruction,
    public BoxInputsPolicy
{
    CompilerRootPropertyName name_;
    bool idempotent_;
    bool callprop_;

    MCallGetProperty(MDefinition *value, PropertyName *name, bool callprop)
      : MUnaryInstruction(value), name_(name),
        idempotent_(false),
        callprop_(callprop)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CallGetProperty)

    static MCallGetProperty *New(TempAllocator &alloc, MDefinition *value, PropertyName *name,
                                 bool callprop)
    {
        return new(alloc) MCallGetProperty(value, name, callprop);
    }
    MDefinition *value() const {
        return getOperand(0);
    }
    PropertyName *name() const {
        return name_;
    }
    bool callprop() const {
        return callprop_;
    }
    TypePolicy *typePolicy() {
        return this;
    }

    // Constructors need to perform a GetProp on the function prototype.
    // Since getters cannot be set on the prototype, fetching is non-effectful.
    // The operation may be safely repeated in case of bailout.
    void setIdempotent() {
        idempotent_ = true;
    }
    AliasSet getAliasSet() const {
        if (!idempotent_)
            return AliasSet::Store(AliasSet::Any);
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Inline call to handle lhs[rhs]. The first input is a Value so that this
// instruction can handle both objects and strings.
class MCallGetElement
  : public MBinaryInstruction,
    public BoxInputsPolicy
{
    MCallGetElement(MDefinition *lhs, MDefinition *rhs)
      : MBinaryInstruction(lhs, rhs)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CallGetElement)

    static MCallGetElement *New(TempAllocator &alloc, MDefinition *lhs, MDefinition *rhs) {
        return new(alloc) MCallGetElement(lhs, rhs);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MCallSetElement
  : public MSetElementInstruction,
    public CallSetElementPolicy
{
    MCallSetElement(MDefinition *object, MDefinition *index, MDefinition *value)
      : MSetElementInstruction(object, index, value)
    {
    }

  public:
    INSTRUCTION_HEADER(CallSetElement)

    static MCallSetElement *New(TempAllocator &alloc, MDefinition *object, MDefinition *index,
                                MDefinition *value)
    {
        return new(alloc) MCallSetElement(object, index, value);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MCallInitElementArray
  : public MAryInstruction<2>,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
    uint32_t index_;

    MCallInitElementArray(MDefinition *obj, uint32_t index, MDefinition *val)
      : index_(index)
    {
        setOperand(0, obj);
        setOperand(1, val);
    }

  public:
    INSTRUCTION_HEADER(CallInitElementArray)

    static MCallInitElementArray *New(TempAllocator &alloc, MDefinition *obj, uint32_t index,
                                      MDefinition *val)
    {
        return new(alloc) MCallInitElementArray(obj, index, val);
    }

    MDefinition *object() const {
        return getOperand(0);
    }

    uint32_t index() const {
        return index_;
    }

    MDefinition *value() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MSetDOMProperty
  : public MAryInstruction<2>,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
    const JSJitSetterOp func_;

    MSetDOMProperty(const JSJitSetterOp func, MDefinition *obj, MDefinition *val)
      : func_(func)
    {
        setOperand(0, obj);
        setOperand(1, val);
    }

  public:
    INSTRUCTION_HEADER(SetDOMProperty)

    static MSetDOMProperty *New(TempAllocator &alloc, const JSJitSetterOp func, MDefinition *obj,
                                MDefinition *val)
    {
        return new(alloc) MSetDOMProperty(func, obj, val);
    }

    const JSJitSetterOp fun() {
        return func_;
    }

    MDefinition *object() {
        return getOperand(0);
    }

    MDefinition *value()
    {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool possiblyCalls() const {
        return true;
    }
};

class MGetDOMProperty
  : public MAryInstruction<2>,
    public ObjectPolicy<0>
{
    const JSJitInfo *info_;

  protected:
    MGetDOMProperty(const JSJitInfo *jitinfo, MDefinition *obj, MDefinition *guard)
      : info_(jitinfo)
    {
        JS_ASSERT(jitinfo);
        JS_ASSERT(jitinfo->type == JSJitInfo::Getter);

        setOperand(0, obj);

        // Pin the guard as an operand if we want to hoist later
        setOperand(1, guard);

        // We are movable iff the jitinfo says we can be.
        if (isDomMovable()) {
            JS_ASSERT(jitinfo->aliasSet != JSJitInfo::AliasEverything);
            setMovable();
        }

        setResultType(MIRType_Value);
    }

    const JSJitInfo *info() const {
        return info_;
    }

  public:
    INSTRUCTION_HEADER(GetDOMProperty)

    static MGetDOMProperty *New(TempAllocator &alloc, const JSJitInfo *info, MDefinition *obj,
                                MDefinition *guard)
    {
        return new(alloc) MGetDOMProperty(info, obj, guard);
    }

    const JSJitGetterOp fun() {
        return info_->getter;
    }
    bool isInfallible() const {
        return info_->isInfallible;
    }
    bool isDomMovable() const {
        return info_->isMovable;
    }
    JSJitInfo::AliasSet domAliasSet() const {
        return info_->aliasSet;
    }
    size_t domMemberSlotIndex() const {
        MOZ_ASSERT(info_->isInSlot);
        return info_->slotIndex;
    }
    MDefinition *object() {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool congruentTo(MDefinition *ins) const {
        if (!isDomMovable())
            return false;

        if (!ins->isGetDOMProperty())
            return false;

        // Checking the jitinfo is the same as checking the constant function
        if (!(info() == ins->toGetDOMProperty()->info()))
            return false;

        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        JSJitInfo::AliasSet aliasSet = domAliasSet();
        if (aliasSet == JSJitInfo::AliasNone)
            return AliasSet::None();
        if (aliasSet == JSJitInfo::AliasDOMSets)
            return AliasSet::Load(AliasSet::DOMProperty);
        JS_ASSERT(aliasSet == JSJitInfo::AliasEverything);
        return AliasSet::Store(AliasSet::Any);
    }

    bool possiblyCalls() const {
        return true;
    }
};

class MGetDOMMember : public MGetDOMProperty
{
    // We inherit everything from MGetDOMProperty except our possiblyCalls value
    MGetDOMMember(const JSJitInfo *jitinfo, MDefinition *obj, MDefinition *guard)
        : MGetDOMProperty(jitinfo, obj, guard)
    {
    }

  public:
    INSTRUCTION_HEADER(GetDOMMember)

    static MGetDOMMember *New(TempAllocator &alloc, const JSJitInfo *info, MDefinition *obj,
                              MDefinition *guard)
    {
        return new(alloc) MGetDOMMember(info, obj, guard);
    }

    bool possiblyCalls() const {
        return false;
    }
};

class MStringLength
  : public MUnaryInstruction,
    public StringPolicy<0>
{
    MStringLength(MDefinition *string)
      : MUnaryInstruction(string)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }
  public:
    INSTRUCTION_HEADER(StringLength)

    static MStringLength *New(TempAllocator &alloc, MDefinition *string) {
        return new(alloc) MStringLength(string);
    }

    MDefinition *foldsTo(TempAllocator &alloc, bool useValueNumbers);

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *string() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // The string |length| property is immutable, so there is no
        // implicit dependency.
        return AliasSet::None();
    }

    void computeRange(TempAllocator &alloc);
};

// Inlined version of Math.floor().
class MFloor
  : public MUnaryInstruction,
    public FloatingPointPolicy<0>
{
    MFloor(MDefinition *num)
      : MUnaryInstruction(num)
    {
        setResultType(MIRType_Int32);
        setPolicyType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Floor)

    static MFloor *New(TempAllocator &alloc, MDefinition *num) {
        return new(alloc) MFloor(num);
    }

    MDefinition *num() const {
        return getOperand(0);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool isFloat32Commutative() const {
        return true;
    }
    void trySpecializeFloat32(TempAllocator &alloc);
#ifdef DEBUG
    bool isConsistentFloat32Use() const {
        return true;
    }
#endif
};

// Inlined version of Math.round().
class MRound
  : public MUnaryInstruction,
    public DoublePolicy<0>
{
    MRound(MDefinition *num)
      : MUnaryInstruction(num)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Round)

    static MRound *New(TempAllocator &alloc, MDefinition *num) {
        return new(alloc) MRound(num);
    }

    MDefinition *num() const {
        return getOperand(0);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

class MIteratorStart
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    uint8_t flags_;

    MIteratorStart(MDefinition *obj, uint8_t flags)
      : MUnaryInstruction(obj), flags_(flags)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(IteratorStart)

    static MIteratorStart *New(TempAllocator &alloc, MDefinition *obj, uint8_t flags) {
        return new(alloc) MIteratorStart(obj, flags);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    uint8_t flags() const {
        return flags_;
    }
};

class MIteratorNext
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MIteratorNext(MDefinition *iter)
      : MUnaryInstruction(iter)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(IteratorNext)

    static MIteratorNext *New(TempAllocator &alloc, MDefinition *iter) {
        return new(alloc) MIteratorNext(iter);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *iterator() const {
        return getOperand(0);
    }
};

class MIteratorMore
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MIteratorMore(MDefinition *iter)
      : MUnaryInstruction(iter)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(IteratorMore)

    static MIteratorMore *New(TempAllocator &alloc, MDefinition *iter) {
        return new(alloc) MIteratorMore(iter);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *iterator() const {
        return getOperand(0);
    }
};

class MIteratorEnd
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MIteratorEnd(MDefinition *iter)
      : MUnaryInstruction(iter)
    { }

  public:
    INSTRUCTION_HEADER(IteratorEnd)

    static MIteratorEnd *New(TempAllocator &alloc, MDefinition *iter) {
        return new(alloc) MIteratorEnd(iter);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *iterator() const {
        return getOperand(0);
    }
};

// Implementation for 'in' operator.
class MIn
  : public MBinaryInstruction,
    public MixPolicy<BoxPolicy<0>, ObjectPolicy<1> >
{
    MIn(MDefinition *key, MDefinition *obj)
      : MBinaryInstruction(key, obj)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(In)

    static MIn *New(TempAllocator &alloc, MDefinition *key, MDefinition *obj) {
        return new(alloc) MIn(key, obj);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool possiblyCalls() const {
        return true;
    }
};


// Test whether the index is in the array bounds or a hole.
class MInArray
  : public MQuaternaryInstruction,
    public ObjectPolicy<3>
{
    bool needsHoleCheck_;
    bool needsNegativeIntCheck_;

    MInArray(MDefinition *elements, MDefinition *index,
             MDefinition *initLength, MDefinition *object,
             bool needsHoleCheck)
      : MQuaternaryInstruction(elements, index, initLength, object),
        needsHoleCheck_(needsHoleCheck),
        needsNegativeIntCheck_(true)
    {
        setResultType(MIRType_Boolean);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(initLength->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(InArray)

    static MInArray *New(TempAllocator &alloc, MDefinition *elements, MDefinition *index,
                         MDefinition *initLength, MDefinition *object,
                         bool needsHoleCheck)
    {
        return new(alloc) MInArray(elements, index, initLength, object, needsHoleCheck);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    MDefinition *index() const {
        return getOperand(1);
    }
    MDefinition *initLength() const {
        return getOperand(2);
    }
    MDefinition *object() const {
        return getOperand(3);
    }
    bool needsHoleCheck() const {
        return needsHoleCheck_;
    }
    bool needsNegativeIntCheck() const {
        return needsNegativeIntCheck_;
    }
    void collectRangeInfoPreTrunc();
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::Element);
    }
    TypePolicy *typePolicy() {
        return this;
    }

};

// Implementation for instanceof operator with specific rhs.
class MInstanceOf
  : public MUnaryInstruction,
    public InstanceOfPolicy
{
    CompilerRootObject protoObj_;

    MInstanceOf(MDefinition *obj, JSObject *proto)
      : MUnaryInstruction(obj),
        protoObj_(proto)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(InstanceOf)

    static MInstanceOf *New(TempAllocator &alloc, MDefinition *obj, JSObject *proto) {
        return new(alloc) MInstanceOf(obj, proto);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    JSObject *prototypeObject() {
        return protoObj_;
    }
};

// Implementation for instanceof operator with unknown rhs.
class MCallInstanceOf
  : public MBinaryInstruction,
    public MixPolicy<BoxPolicy<0>, ObjectPolicy<1> >
{
    MCallInstanceOf(MDefinition *obj, MDefinition *proto)
      : MBinaryInstruction(obj, proto)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(CallInstanceOf)

    static MCallInstanceOf *New(TempAllocator &alloc, MDefinition *obj, MDefinition *proto) {
        return new(alloc) MCallInstanceOf(obj, proto);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MArgumentsLength : public MNullaryInstruction
{
    MArgumentsLength()
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ArgumentsLength)

    static MArgumentsLength *New(TempAllocator &alloc) {
        return new(alloc) MArgumentsLength();
    }

    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // Arguments |length| cannot be mutated by Ion Code.
        return AliasSet::None();
   }

    void computeRange(TempAllocator &alloc);
};

// This MIR instruction is used to get an argument from the actual arguments.
class MGetFrameArgument
  : public MUnaryInstruction,
    public IntPolicy<0>
{
    bool scriptHasSetArg_;

    MGetFrameArgument(MDefinition *idx, bool scriptHasSetArg)
      : MUnaryInstruction(idx),
        scriptHasSetArg_(scriptHasSetArg)
    {
        setResultType(MIRType_Value);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(GetFrameArgument)

    static MGetFrameArgument *New(TempAllocator &alloc, MDefinition *idx, bool scriptHasSetArg) {
        return new(alloc) MGetFrameArgument(idx, scriptHasSetArg);
    }

    MDefinition *index() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // If the script doesn't have any JSOP_SETARG ops, then this instruction is never
        // aliased.
        if (scriptHasSetArg_)
            return AliasSet::Load(AliasSet::FrameArgument);
        return AliasSet::None();
    }
};

// This MIR instruction is used to set an argument value in the frame.
class MSetFrameArgument
  : public MUnaryInstruction,
    public NoFloatPolicy<0>
{
    uint32_t argno_;

    MSetFrameArgument(uint32_t argno, MDefinition *value)
      : MUnaryInstruction(value),
        argno_(argno)
    {
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(SetFrameArgument)

    static MSetFrameArgument *New(TempAllocator &alloc, uint32_t argno, MDefinition *value) {
        return new(alloc) MSetFrameArgument(argno, value);
    }

    uint32_t argno() const {
        return argno_;
    }

    MDefinition *value() const {
        return getOperand(0);
    }

    bool congruentTo(MDefinition *ins) const {
        return false;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::FrameArgument);
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

class MRestCommon
{
    unsigned numFormals_;
    CompilerRootObject templateObject_;

  protected:
    MRestCommon(unsigned numFormals, JSObject *templateObject)
      : numFormals_(numFormals),
        templateObject_(templateObject)
   { }

  public:
    unsigned numFormals() const {
        return numFormals_;
    }
    JSObject *templateObject() const {
        return templateObject_;
    }
};

class MRest
  : public MUnaryInstruction,
    public MRestCommon,
    public IntPolicy<0>
{
    MRest(types::CompilerConstraintList *constraints, MDefinition *numActuals, unsigned numFormals,
          JSObject *templateObject)
      : MUnaryInstruction(numActuals),
        MRestCommon(numFormals, templateObject)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(constraints, templateObject));
    }

  public:
    INSTRUCTION_HEADER(Rest);

    static MRest *New(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                      MDefinition *numActuals, unsigned numFormals,
                      JSObject *templateObject)
    {
        return new(alloc) MRest(constraints, numActuals, numFormals, templateObject);
    }

    MDefinition *numActuals() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MRestPar
  : public MBinaryInstruction,
    public MRestCommon,
    public IntPolicy<1>
{
    MRestPar(MDefinition *slice, MDefinition *numActuals, unsigned numFormals,
             JSObject *templateObject, types::TemporaryTypeSet *resultTypes)
      : MBinaryInstruction(slice, numActuals),
        MRestCommon(numFormals, templateObject)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(resultTypes);
    }

  public:
    INSTRUCTION_HEADER(RestPar);

    static MRestPar *New(TempAllocator &alloc, MDefinition *slice, MRest *rest) {
        return new(alloc) MRestPar(slice, rest->numActuals(), rest->numFormals(),
                                   rest->templateObject(), rest->resultTypeSet());
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }
    MDefinition *numActuals() const {
        return getOperand(1);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Guard on an object being allocated in the current slice.
class MGuardThreadLocalObject
  : public MBinaryInstruction,
    public ObjectPolicy<1>
{
    MGuardThreadLocalObject(MDefinition *slice, MDefinition *obj)
      : MBinaryInstruction(slice, obj)
    {
        setResultType(MIRType_None);
        setGuard();
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(GuardThreadLocalObject);

    static MGuardThreadLocalObject *New(TempAllocator &alloc, MDefinition *slice, MDefinition *obj) {
        return new(alloc) MGuardThreadLocalObject(slice, obj);
    }
    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }
    MDefinition *object() const {
        return getOperand(1);
    }
    BailoutKind bailoutKind() const {
        return Bailout_Normal;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

// Given a value, guard that the value is in a particular TypeSet, then returns
// that value.
class MTypeBarrier
  : public MUnaryInstruction,
    public TypeBarrierPolicy
{
    MTypeBarrier(MDefinition *def, types::TemporaryTypeSet *types)
      : MUnaryInstruction(def)
    {
        JS_ASSERT(!types->unknown());

        MIRType type = MIRTypeFromValueType(types->getKnownTypeTag());
        setResultType(type);
        setResultTypeSet(types);

        setGuard();
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TypeBarrier)

    static MTypeBarrier *New(TempAllocator &alloc, MDefinition *def, types::TemporaryTypeSet *types) {
        return new(alloc) MTypeBarrier(def, types);
    }

    void printOpcode(FILE *fp) const;

    TypePolicy *typePolicy() {
        return this;
    }

    bool congruentTo(MDefinition *def) const {
        return false;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    virtual bool neverHoist() const {
        return resultTypeSet()->empty();
    }

    bool alwaysBails() const {
        // If mirtype of input doesn't agree with mirtype of barrier,
        // we will definitely bail.
        MIRType type = MIRTypeFromValueType(resultTypeSet()->getKnownTypeTag());
        if (type == MIRType_Value)
            return false;
        if (input()->type() == MIRType_Value)
            return false;
        return input()->type() != type;
    }
};

// Like MTypeBarrier, guard that the value is in the given type set. This is
// used before property writes to ensure the value being written is represented
// in the property types for the object.
class MMonitorTypes : public MUnaryInstruction, public BoxInputsPolicy
{
    const types::TemporaryTypeSet *typeSet_;

    MMonitorTypes(MDefinition *def, const types::TemporaryTypeSet *types)
      : MUnaryInstruction(def),
        typeSet_(types)
    {
        setGuard();
        JS_ASSERT(!types->unknown());
    }

  public:
    INSTRUCTION_HEADER(MonitorTypes)

    static MMonitorTypes *New(TempAllocator &alloc, MDefinition *def, const types::TemporaryTypeSet *types) {
        return new(alloc) MMonitorTypes(def, types);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    const types::TemporaryTypeSet *typeSet() const {
        return typeSet_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Given a value being written to another object, update the generational store
// buffer if the value is in the nursery and object is in the tenured heap.
class MPostWriteBarrier
  : public MBinaryInstruction,
    public ObjectPolicy<0>
{
    bool hasValue_;

    MPostWriteBarrier(MDefinition *obj, MDefinition *value)
      : MBinaryInstruction(obj, value), hasValue_(true)
    {
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(PostWriteBarrier)

    static MPostWriteBarrier *New(TempAllocator &alloc, MDefinition *obj, MDefinition *value) {
        return new(alloc) MPostWriteBarrier(obj, value);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *object() const {
        return getOperand(0);
    }

    bool hasValue() const {
        return hasValue_;
    }
    MDefinition *value() const {
        return getOperand(1);
    }
#ifdef DEBUG
    bool isConsistentFloat32Use() const {
        // During lowering, values that neither have object nor value MIR type
        // are ignored, thus Float32 can show up at this point without any issue.
        return true;
    }
#endif
};

class MNewSlots : public MNullaryInstruction
{
    unsigned nslots_;

    MNewSlots(unsigned nslots)
      : nslots_(nslots)
    {
        setResultType(MIRType_Slots);
    }

  public:
    INSTRUCTION_HEADER(NewSlots)

    static MNewSlots *New(TempAllocator &alloc, unsigned nslots) {
        return new(alloc) MNewSlots(nslots);
    }
    unsigned nslots() const {
        return nslots_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    bool possiblyCalls() const {
        return true;
    }
};

class MNewDeclEnvObject : public MNullaryInstruction
{
    CompilerRootObject templateObj_;

    MNewDeclEnvObject(JSObject *templateObj)
      : MNullaryInstruction(),
        templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewDeclEnvObject);

    static MNewDeclEnvObject *New(TempAllocator &alloc, JSObject *templateObj) {
        return new(alloc) MNewDeclEnvObject(templateObj);
    }

    JSObject *templateObj() {
        return templateObj_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MNewCallObject : public MUnaryInstruction
{
    CompilerRootObject templateObj_;
    bool needsSingletonType_;

    MNewCallObject(JSObject *templateObj, bool needsSingletonType, MDefinition *slots)
      : MUnaryInstruction(slots),
        templateObj_(templateObj),
        needsSingletonType_(needsSingletonType)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewCallObject)

    static MNewCallObject *New(TempAllocator &alloc, JSObject *templateObj, bool needsSingletonType,
                               MDefinition *slots)
    {
        return new(alloc) MNewCallObject(templateObj, needsSingletonType, slots);
    }

    MDefinition *slots() {
        return getOperand(0);
    }
    JSObject *templateObject() {
        return templateObj_;
    }
    bool needsSingletonType() const {
        return needsSingletonType_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MNewCallObjectPar : public MBinaryInstruction
{
    CompilerRootObject templateObj_;

    MNewCallObjectPar(MDefinition *slice, JSObject *templateObj, MDefinition *slots)
        : MBinaryInstruction(slice, slots),
          templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewCallObjectPar);

    static MNewCallObjectPar *New(TempAllocator &alloc, MDefinition *slice, MNewCallObject *callObj) {
        return new(alloc) MNewCallObjectPar(slice, callObj->templateObject(), callObj->slots());
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }

    MDefinition *slots() const {
        return getOperand(1);
    }

    JSObject *templateObj() const {
        return templateObj_;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MNewStringObject :
  public MUnaryInstruction,
  public StringPolicy<0>
{
    CompilerRootObject templateObj_;

    MNewStringObject(MDefinition *input, JSObject *templateObj)
      : MUnaryInstruction(input),
        templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewStringObject)

    static MNewStringObject *New(TempAllocator &alloc, MDefinition *input, JSObject *templateObj) {
        return new(alloc) MNewStringObject(input, templateObj);
    }

    StringObject *templateObj() const;

    TypePolicy *typePolicy() {
        return this;
    }
};

// Node that represents that a script has begun executing. This comes at the
// start of the function and is called once per function (including inline
// ones)
class MFunctionBoundary : public MNullaryInstruction
{
  public:
    enum Type {
        Enter,        // a function has begun executing and it is not inline
        Exit,         // any function has exited (inlined or normal)
        Inline_Enter, // an inline function has begun executing

        Inline_Exit   // all instructions of an inline function are done, a
                      // return from the inline function could have occurred
                      // before this boundary
    };

  private:
    JSScript *script_;
    Type type_;
    unsigned inlineLevel_;

    MFunctionBoundary(JSScript *script, Type type, unsigned inlineLevel)
      : script_(script), type_(type), inlineLevel_(inlineLevel)
    {
        JS_ASSERT_IF(type != Inline_Exit, script != nullptr);
        JS_ASSERT_IF(type == Inline_Enter, inlineLevel != 0);
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(FunctionBoundary)

    static MFunctionBoundary *New(TempAllocator &alloc, JSScript *script, Type type,
                                  unsigned inlineLevel = 0) {
        return new(alloc) MFunctionBoundary(script, type, inlineLevel);
    }

    JSScript *script() {
        return script_;
    }

    Type type() {
        return type_;
    }

    unsigned inlineLevel() {
        return inlineLevel_;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// This is an alias for MLoadFixedSlot.
class MEnclosingScope : public MLoadFixedSlot
{
    MEnclosingScope(MDefinition *obj)
      : MLoadFixedSlot(obj, ScopeObject::enclosingScopeSlot())
    {
        setResultType(MIRType_Object);
    }

  public:
    static MEnclosingScope *New(TempAllocator &alloc, MDefinition *obj) {
        return new(alloc) MEnclosingScope(obj);
    }

    AliasSet getAliasSet() const {
        // ScopeObject reserved slots are immutable.
        return AliasSet::None();
    }
};

// Creates a dense array of the given length.
//
// Note: the template object should be an *empty* dense array!
class MNewDenseArrayPar : public MBinaryInstruction
{
    CompilerRootObject templateObject_;

    MNewDenseArrayPar(MDefinition *slice, MDefinition *length, JSObject *templateObject)
      : MBinaryInstruction(slice, length),
        templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewDenseArrayPar);

    static MNewDenseArrayPar *New(TempAllocator &alloc, MDefinition *slice, MDefinition *length,
                                  JSObject *templateObject)
    {
        return new(alloc) MNewDenseArrayPar(slice, length, templateObject);
    }

    MDefinition *forkJoinSlice() const {
        return getOperand(0);
    }

    MDefinition *length() const {
        return getOperand(1);
    }

    JSObject *templateObject() const {
        return templateObject_;
    }

    bool possiblyCalls() const {
        return true;
    }
};

// A resume point contains the information needed to reconstruct the interpreter
// state from a position in the JIT. See the big comment near resumeAfter() in
// IonBuilder.cpp.
class MResumePoint MOZ_FINAL : public MNode, public InlineForwardListNode<MResumePoint>
{
  public:
    enum Mode {
        ResumeAt,    // Resume until before the current instruction
        ResumeAfter, // Resume after the current instruction
        Outer        // State before inlining.
    };

  private:
    friend class MBasicBlock;
    friend void AssertBasicGraphCoherency(MIRGraph &graph);

    FixedList<MUse> operands_;
    uint32_t stackDepth_;
    jsbytecode *pc_;
    MResumePoint *caller_;
    MInstruction *instruction_;
    Mode mode_;

    MResumePoint(MBasicBlock *block, jsbytecode *pc, MResumePoint *parent, Mode mode);
    void inherit(MBasicBlock *state);

  protected:
    // Initializes operands_ to an empty array of a fixed length.
    // The array may then be filled in by inherit().
    bool init(TempAllocator &alloc) {
        return operands_.init(alloc, stackDepth_);
    }

    // Overwrites an operand without updating its Uses.
    void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index < stackDepth_);
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }

    void clearOperand(size_t index) {
        JS_ASSERT(index < stackDepth_);
        operands_[index].set(nullptr, this, index);
    }

    MUse *getUseFor(size_t index) {
        return &operands_[index];
    }

  public:
    static MResumePoint *New(TempAllocator &alloc, MBasicBlock *block, jsbytecode *pc,
                             MResumePoint *parent, Mode mode);

    MNode::Kind kind() const {
        return MNode::ResumePoint;
    }
    size_t numOperands() const {
        return stackDepth_;
    }
    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index < stackDepth_);
        return operands_[index].producer();
    }
    jsbytecode *pc() const {
        return pc_;
    }
    uint32_t stackDepth() const {
        return stackDepth_;
    }
    MResumePoint *caller() {
        return caller_;
    }
    void setCaller(MResumePoint *caller) {
        caller_ = caller;
    }
    uint32_t frameCount() const {
        uint32_t count = 1;
        for (MResumePoint *it = caller_; it; it = it->caller_)
            count++;
        return count;
    }
    MInstruction *instruction() {
        return instruction_;
    }
    void setInstruction(MInstruction *ins) {
        instruction_ = ins;
    }
    Mode mode() const {
        return mode_;
    }

    void discardUses() {
        for (size_t i = 0; i < stackDepth_; i++) {
            if (operands_[i].hasProducer())
                operands_[i].producer()->removeUse(&operands_[i]);
        }
    }
};

/*
 * Facade for a chain of MResumePoints that cross frame boundaries (due to
 * function inlining). Operands are ordered from oldest frame to newest.
 */
class FlattenedMResumePointIter
{
    Vector<MResumePoint *, 8, SystemAllocPolicy> resumePoints;
    MResumePoint *newest;
    size_t numOperands_;

  public:
    explicit FlattenedMResumePointIter(MResumePoint *newest)
      : newest(newest), numOperands_(0)
    {}

    bool init() {
        MResumePoint *it = newest;
        do {
            if (!resumePoints.append(it))
                return false;
            it = it->caller();
        } while (it);
        Reverse(resumePoints.begin(), resumePoints.end());
        return true;
    }

    MResumePoint **begin() {
        return resumePoints.begin();
    }
    MResumePoint **end() {
        return resumePoints.end();
    }

    size_t numOperands() const {
        return numOperands_;
    }
};

class MIsCallable
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    MIsCallable(MDefinition *object)
      : MUnaryInstruction(object)
    {
        setResultType(MIRType_Boolean);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(IsCallable);

    static MIsCallable *New(TempAllocator &alloc, MDefinition *obj) {
        return new(alloc) MIsCallable(obj);
    }

    MDefinition *object() const {
        return getOperand(0);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MHaveSameClass
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1> >
{
    MHaveSameClass(MDefinition *left, MDefinition *right)
      : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Boolean);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(HaveSameClass);

    static MHaveSameClass *New(TempAllocator &alloc, MDefinition *left, MDefinition *right) {
        return new(alloc) MHaveSameClass(left, right);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Increase the usecount of the provided script upon execution and test if
// the usecount surpasses the threshold. Upon hit it will recompile the
// outermost script (i.e. not the inlined script).
class MRecompileCheck : public MNullaryInstruction
{
    JSScript *script_;
    uint32_t recompileThreshold_;

    MRecompileCheck(JSScript *script, uint32_t recompileThreshold)
      : script_(script),
        recompileThreshold_(recompileThreshold)
    {
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(RecompileCheck);

    static MRecompileCheck *New(TempAllocator &alloc, JSScript *script_, uint32_t useCount) {
        return new(alloc) MRecompileCheck(script_, useCount);
    }

    JSScript *script() const {
        return script_;
    }

    uint32_t recompileThreshold() const {
        return recompileThreshold_;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MAsmJSNeg : public MUnaryInstruction
{
    MAsmJSNeg(MDefinition *op, MIRType type)
      : MUnaryInstruction(op)
    {
        setResultType(type);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(AsmJSNeg);
    static MAsmJSNeg *NewAsmJS(TempAllocator &alloc, MDefinition *op, MIRType type) {
        return new(alloc) MAsmJSNeg(op, type);
    }
};

class MAsmJSHeapAccess
{
    ArrayBufferView::ViewType viewType_;
    bool skipBoundsCheck_;

  public:
    MAsmJSHeapAccess(ArrayBufferView::ViewType vt, bool s)
      : viewType_(vt), skipBoundsCheck_(s)
    {}

    ArrayBufferView::ViewType viewType() const { return viewType_; }
    bool skipBoundsCheck() const { return skipBoundsCheck_; }
    void setSkipBoundsCheck(bool v) { skipBoundsCheck_ = v; }
};

class MAsmJSLoadHeap : public MUnaryInstruction, public MAsmJSHeapAccess
{
    MAsmJSLoadHeap(ArrayBufferView::ViewType vt, MDefinition *ptr)
      : MUnaryInstruction(ptr), MAsmJSHeapAccess(vt, false)
    {
        setMovable();
        if (vt == ArrayBufferView::TYPE_FLOAT32)
            setResultType(MIRType_Float32);
        else if (vt == ArrayBufferView::TYPE_FLOAT64)
            setResultType(MIRType_Double);
        else
            setResultType(MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(AsmJSLoadHeap);

    static MAsmJSLoadHeap *New(TempAllocator &alloc, ArrayBufferView::ViewType vt, MDefinition *ptr) {
        return new(alloc) MAsmJSLoadHeap(vt, ptr);
    }

    MDefinition *ptr() const { return getOperand(0); }

    bool congruentTo(MDefinition *ins) const;
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::AsmJSHeap);
    }
    bool mightAlias(MDefinition *def);
};

class MAsmJSStoreHeap : public MBinaryInstruction, public MAsmJSHeapAccess
{
    MAsmJSStoreHeap(ArrayBufferView::ViewType vt, MDefinition *ptr, MDefinition *v)
      : MBinaryInstruction(ptr, v) , MAsmJSHeapAccess(vt, false)
    {}

  public:
    INSTRUCTION_HEADER(AsmJSStoreHeap);

    static MAsmJSStoreHeap *New(TempAllocator &alloc, ArrayBufferView::ViewType vt,
                                MDefinition *ptr, MDefinition *v)
    {
        return new(alloc) MAsmJSStoreHeap(vt, ptr, v);
    }

    MDefinition *ptr() const { return getOperand(0); }
    MDefinition *value() const { return getOperand(1); }

    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::AsmJSHeap);
    }
};

class MAsmJSLoadGlobalVar : public MNullaryInstruction
{
    MAsmJSLoadGlobalVar(MIRType type, unsigned globalDataOffset, bool isConstant)
      : globalDataOffset_(globalDataOffset), isConstant_(isConstant)
    {
        JS_ASSERT(IsNumberType(type));
        setResultType(type);
        setMovable();
    }

    unsigned globalDataOffset_;
    bool isConstant_;

  public:
    INSTRUCTION_HEADER(AsmJSLoadGlobalVar);

    static MAsmJSLoadGlobalVar *New(TempAllocator &alloc, MIRType type, unsigned globalDataOffset,
                                    bool isConstant)
    {
        return new(alloc) MAsmJSLoadGlobalVar(type, globalDataOffset, isConstant);
    }

    unsigned globalDataOffset() const { return globalDataOffset_; }

    bool congruentTo(MDefinition *ins) const;

    AliasSet getAliasSet() const {
        return isConstant_ ? AliasSet::None() : AliasSet::Load(AliasSet::AsmJSGlobalVar);
    }

    bool mightAlias(MDefinition *def);
};

class MAsmJSStoreGlobalVar : public MUnaryInstruction
{
    MAsmJSStoreGlobalVar(unsigned globalDataOffset, MDefinition *v)
      : MUnaryInstruction(v), globalDataOffset_(globalDataOffset)
    {}

    unsigned globalDataOffset_;

  public:
    INSTRUCTION_HEADER(AsmJSStoreGlobalVar);

    static MAsmJSStoreGlobalVar *New(TempAllocator &alloc, unsigned globalDataOffset, MDefinition *v) {
        return new(alloc) MAsmJSStoreGlobalVar(globalDataOffset, v);
    }

    unsigned globalDataOffset() const { return globalDataOffset_; }
    MDefinition *value() const { return getOperand(0); }

    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::AsmJSGlobalVar);
    }
};

class MAsmJSLoadFuncPtr : public MUnaryInstruction
{
    MAsmJSLoadFuncPtr(unsigned globalDataOffset, MDefinition *index)
      : MUnaryInstruction(index), globalDataOffset_(globalDataOffset)
    {
        setResultType(MIRType_Pointer);
    }

    unsigned globalDataOffset_;

  public:
    INSTRUCTION_HEADER(AsmJSLoadFuncPtr);

    static MAsmJSLoadFuncPtr *New(TempAllocator &alloc, unsigned globalDataOffset,
                                  MDefinition *index)
    {
        return new(alloc) MAsmJSLoadFuncPtr(globalDataOffset, index);
    }

    unsigned globalDataOffset() const { return globalDataOffset_; }
    MDefinition *index() const { return getOperand(0); }
};

class MAsmJSLoadFFIFunc : public MNullaryInstruction
{
    MAsmJSLoadFFIFunc(unsigned globalDataOffset)
      : globalDataOffset_(globalDataOffset)
    {
        setResultType(MIRType_Pointer);
    }

    unsigned globalDataOffset_;

  public:
    INSTRUCTION_HEADER(AsmJSLoadFFIFunc);

    static MAsmJSLoadFFIFunc *New(TempAllocator &alloc, unsigned globalDataOffset)
    {
        return new(alloc) MAsmJSLoadFFIFunc(globalDataOffset);
    }

    unsigned globalDataOffset() const { return globalDataOffset_; }
};

class MAsmJSParameter : public MNullaryInstruction
{
    ABIArg abi_;

    MAsmJSParameter(ABIArg abi, MIRType mirType)
      : abi_(abi)
    {
        setResultType(mirType);
    }

  public:
    INSTRUCTION_HEADER(AsmJSParameter);

    static MAsmJSParameter *New(TempAllocator &alloc, ABIArg abi, MIRType mirType) {
        return new(alloc) MAsmJSParameter(abi, mirType);
    }

    ABIArg abi() const { return abi_; }
};

class MAsmJSReturn : public MAryControlInstruction<1, 0>
{
    MAsmJSReturn(MDefinition *ins) {
        setOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(AsmJSReturn);
    static MAsmJSReturn *New(TempAllocator &alloc, MDefinition *ins) {
        return new(alloc) MAsmJSReturn(ins);
    }
};

class MAsmJSVoidReturn : public MAryControlInstruction<0, 0>
{
  public:
    INSTRUCTION_HEADER(AsmJSVoidReturn);
    static MAsmJSVoidReturn *New(TempAllocator &alloc) {
        return new(alloc) MAsmJSVoidReturn();
    }
};

class MAsmJSPassStackArg : public MUnaryInstruction
{
    MAsmJSPassStackArg(uint32_t spOffset, MDefinition *ins)
      : MUnaryInstruction(ins),
        spOffset_(spOffset)
    {}

    uint32_t spOffset_;

  public:
    INSTRUCTION_HEADER(AsmJSPassStackArg);
    static MAsmJSPassStackArg *New(TempAllocator &alloc, uint32_t spOffset, MDefinition *ins) {
        return new(alloc) MAsmJSPassStackArg(spOffset, ins);
    }
    uint32_t spOffset() const {
        return spOffset_;
    }
    void incrementOffset(uint32_t inc) {
        spOffset_ += inc;
    }
    MDefinition *arg() const {
        return getOperand(0);
    }
};

class MAsmJSCall MOZ_FINAL : public MInstruction
{
  public:
    class Callee {
      public:
        enum Which { Internal, Dynamic, Builtin };
      private:
        Which which_;
        union {
            Label *internal_;
            MDefinition *dynamic_;
            AsmJSImmKind builtin_;
        } u;
      public:
        Callee() {}
        Callee(Label *callee) : which_(Internal) { u.internal_ = callee; }
        Callee(MDefinition *callee) : which_(Dynamic) { u.dynamic_ = callee; }
        Callee(AsmJSImmKind callee) : which_(Builtin) { u.builtin_ = callee; }
        Which which() const { return which_; }
        Label *internal() const { JS_ASSERT(which_ == Internal); return u.internal_; }
        MDefinition *dynamic() const { JS_ASSERT(which_ == Dynamic); return u.dynamic_; }
        AsmJSImmKind builtin() const { JS_ASSERT(which_ == Builtin); return u.builtin_; }
    };

  private:
    struct Operand {
        AnyRegister reg;
        MUse use;
    };

    Callee callee_;
    FixedList<MUse> operands_;
    FixedList<AnyRegister> argRegs_;
    size_t spIncrement_;

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }
    MUse *getUseFor(size_t index) {
        return &operands_[index];
    }

  public:
    INSTRUCTION_HEADER(AsmJSCall);

    struct Arg {
        AnyRegister reg;
        MDefinition *def;
        Arg(AnyRegister reg, MDefinition *def) : reg(reg), def(def) {}
    };
    typedef Vector<Arg, 8> Args;

    static MAsmJSCall *New(TempAllocator &alloc, Callee callee, const Args &args,
                           MIRType resultType, size_t spIncrement);

    size_t numOperands() const {
        return operands_.length();
    }
    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index < numOperands());
        return operands_[index].producer();
    }
    size_t numArgs() const {
        return argRegs_.length();
    }
    AnyRegister registerForArg(size_t index) const {
        JS_ASSERT(index < numArgs());
        return argRegs_[index];
    }
    Callee callee() const {
        return callee_;
    }
    size_t dynamicCalleeOperandIndex() const {
        JS_ASSERT(callee_.which() == Callee::Dynamic);
        JS_ASSERT(numArgs() == numOperands() - 1);
        return numArgs();
    }
    size_t spIncrement() const {
        return spIncrement_;
    }

    bool possiblyCalls() const {
        return true;
    }
};

// The asm.js version doesn't use the bail mechanism: instead it throws and
// exception by jumping to the given label.
class MAsmJSCheckOverRecursed : public MNullaryInstruction
{
    Label *onError_;
    MAsmJSCheckOverRecursed(Label *onError) : onError_(onError) {}
  public:
    INSTRUCTION_HEADER(AsmJSCheckOverRecursed);
    static MAsmJSCheckOverRecursed *New(TempAllocator &alloc, Label *onError) {
        return new(alloc) MAsmJSCheckOverRecursed(onError);
    }
    Label *onError() const { return onError_; }
};

#undef INSTRUCTION_HEADER

// Implement opcode casts now that the compiler can see the inheritance.
#define OPCODE_CASTS(opcode)                                                \
    M##opcode *MDefinition::to##opcode()                                    \
    {                                                                       \
        JS_ASSERT(is##opcode());                                            \
        return static_cast<M##opcode *>(this);                              \
    }                                                                       \
    const M##opcode *MDefinition::to##opcode() const                        \
    {                                                                       \
        JS_ASSERT(is##opcode());                                            \
        return static_cast<const M##opcode *>(this);                        \
    }
MIR_OPCODE_LIST(OPCODE_CASTS)
#undef OPCODE_CASTS

MDefinition *MNode::toDefinition()
{
    JS_ASSERT(isDefinition());
    return (MDefinition *)this;
}

MResumePoint *MNode::toResumePoint()
{
    JS_ASSERT(isResumePoint());
    return (MResumePoint *)this;
}

MInstruction *MDefinition::toInstruction()
{
    JS_ASSERT(!isPhi());
    return (MInstruction *)this;
}

static inline bool isOSRLikeValue (MDefinition *def) {
    if (def->isOsrValue())
        return true;

    if (def->isUnbox())
        if (def->getOperand(0)->isOsrValue())
            return true;

    return false;
}

typedef Vector<MDefinition *, 8, IonAllocPolicy> MDefinitionVector;

// Helper functions used to decide how to build MIR.

bool ElementAccessIsDenseNative(MDefinition *obj, MDefinition *id);
bool ElementAccessIsTypedArray(MDefinition *obj, MDefinition *id,
                               ScalarTypeRepresentation::Type *arrayType);
bool ElementAccessIsPacked(types::CompilerConstraintList *constraints, MDefinition *obj);
bool ElementAccessHasExtraIndexedProperty(types::CompilerConstraintList *constraints,
                                          MDefinition *obj);
MIRType DenseNativeElementType(types::CompilerConstraintList *constraints, MDefinition *obj);
bool PropertyReadNeedsTypeBarrier(JSContext *propertycx,
                                  types::CompilerConstraintList *constraints,
                                  types::TypeObjectKey *object, PropertyName *name,
                                  types::TemporaryTypeSet *observed, bool updateObserved);
bool PropertyReadNeedsTypeBarrier(JSContext *propertycx,
                                  types::CompilerConstraintList *constraints,
                                  MDefinition *obj, PropertyName *name,
                                  types::TemporaryTypeSet *observed);
bool PropertyReadOnPrototypeNeedsTypeBarrier(types::CompilerConstraintList *constraints,
                                             MDefinition *obj, PropertyName *name,
                                             types::TemporaryTypeSet *observed);
bool PropertyReadIsIdempotent(types::CompilerConstraintList *constraints,
                              MDefinition *obj, PropertyName *name);
bool AddObjectsForPropertyRead(MDefinition *obj, PropertyName *name,
                               types::TemporaryTypeSet *observed);
bool PropertyWriteNeedsTypeBarrier(TempAllocator &alloc, types::CompilerConstraintList *constraints,
                                   MBasicBlock *current, MDefinition **pobj,
                                   PropertyName *name, MDefinition **pvalue,
                                   bool canModify);

} // namespace jit
} // namespace js

#endif /* jit_MIR_h */
