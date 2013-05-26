/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_mir_h__
#define jsion_mir_h__

// This file declares everything needed to build actual MIR instructions: the
// actual opcodes and instructions themselves, the instruction interface, and
// use chains.
#include "jscntxt.h"
#include "jslibmath.h"
#include "jsinfer.h"
#include "jsinferinlines.h"
#include "jstypedarrayinlines.h"
#include "TypePolicy.h"
#include "IonAllocPolicy.h"
#include "InlineList.h"
#include "MOpcodes.h"
#include "FixedArityList.h"
#include "IonMacroAssembler.h"
#include "Bailouts.h"
#include "FixedList.h"
#include "CompilerRoot.h"

namespace js {
namespace ion {

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
    _(Folded)        /* Has constant folded uses not reflected in SSA */        \
                                                                                \
    /* The instruction has been marked dead for lazy removal from resume
     * points.
     */                                                                         \
    _(Unused)                                                                   \
    _(DOMFunction)   /* Contains or uses a common DOM method function */

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
      : producer_(NULL), consumer_(NULL), index_(0)
    { }

    static inline MUse *New(MDefinition *producer, MNode *consumer, uint32_t index) {
        return new MUse(producer, consumer, index);
    }

    // Set data inside the MUse.
    void set(MDefinition *producer, MNode *consumer, uint32_t index) {
        producer_ = producer;
        consumer_ = consumer;
        index_ = index;
    }

    MDefinition *producer() const {
        JS_ASSERT(producer_ != NULL);
        return producer_;
    }
    bool hasProducer() const {
        return producer_ != NULL;
    }
    MNode *consumer() const {
        JS_ASSERT(consumer_ != NULL);
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
      : block_(NULL)
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
        return NULL;
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
        Last              = DOMProperty,
        Any               = Last | (Last - 1),

        NumCategories     = 6,

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
    friend class Loop;

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
    types::StackTypeSet *resultTypeSet_; // Optional refinement of the result type.
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

    void setBlock(MBasicBlock *block) {
        block_ = block;
    }

    bool hasFlags(uint32_t flags) const {
        return (flags_ & flags) == flags;
    }
    void removeFlags(uint32_t flags) {
        flags_ &= ~flags;
    }
    void setFlags(uint32_t flags) {
        flags_ |= flags;
    }
    virtual bool neverHoist() const { return false; }
  public:
    MDefinition()
      : id_(0),
        valueNumber_(NULL),
        range_(NULL),
        resultType_(MIRType_None),
        resultTypeSet_(NULL),
        flags_(0),
        dependency_(NULL),
        trackedPc_(NULL)
    { }

    virtual Opcode op() const = 0;
    virtual const char *opName() const = 0;
    void printName(FILE *fp);
    static void PrintOpcodeName(FILE *fp, Opcode op);
    virtual void printOpcode(FILE *fp);

    void setTrackedPc(jsbytecode *pc) {
        trackedPc_ = pc;
    }

    jsbytecode *trackedPc() {
        return trackedPc_;
    }

    Range *range() const {
        return range_;
    }
    void setRange(Range *range) {
        range_ = range;
    }

    virtual HashNumber valueHash() const;
    virtual bool congruentTo(MDefinition* const &ins) const {
        return false;
    }
    bool congruentIfOperandsEqual(MDefinition * const &ins) const;
    virtual MDefinition *foldsTo(bool useValueNumbers);
    virtual void analyzeEdgeCasesForward();
    virtual void analyzeEdgeCasesBackward();

    virtual bool truncate();
    virtual bool isOperandTruncated(size_t index) const;

    bool earlyAbortCheck();

    // Compute an absolute or symbolic range for the value of this node.
    // Ranges are only computed for definitions whose type is int32.
    virtual void computeRange() {
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
    void setValueNumberData(ValueNumberData *vn) {
        JS_ASSERT(valueNumber_ == NULL);
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

    MIRType type() const {
        return resultType_;
    }

    types::StackTypeSet *resultTypeSet() const {
        return resultTypeSet_;
    }

    bool mightBeType(MIRType type) const {
        JS_ASSERT(type != MIRType_Value);

        if (type == this->type())
            return true;

        if (MIRType_Value != this->type())
            return false;

        return !resultTypeSet() || resultTypeSet()->mightBeType(ValueTypeFromMIRType(type));
    }

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

    // Same thing, but for folding
    virtual bool updateForFolding(MDefinition *ins) {
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
    inline M##opcode *to##opcode();
    MIR_OPCODE_LIST(OPCODE_CASTS)
#   undef OPCODE_CASTS

    inline MInstruction *toInstruction();
    bool isInstruction() const {
        return !isPhi();
    }

    void setResultType(MIRType type) {
        resultType_ = type;
    }
    void setResultTypeSet(types::StackTypeSet *types) {
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
      : resumePoint_(NULL)
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
    FixedArityList<MUse, Arity> operands_;

    void setOperand(size_t index, MDefinition *operand) {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }

    MUse *getUseFor(size_t index) {
        return &operands_[index];
    }

  public:
    MDefinition *getOperand(size_t index) const {
        return operands_[index].producer();
    }
    size_t numOperands() const {
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
    static MStart *New(StartType startType) {
        return new MStart(startType);
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
    static MOsrEntry *New() {
        return new MOsrEntry;
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
    static MNop *New() {
        return new MNop();
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
    MConstant(const Value &v);

  public:
    INSTRUCTION_HEADER(Constant)
    static MConstant *New(const Value &v);

    const js::Value &value() const {
        return value_;
    }
    const js::Value *vp() const {
        return &value_;
    }

    void printOpcode(FILE *fp);

    HashNumber valueHash() const;
    bool congruentTo(MDefinition * const &ins) const;

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange();
    bool truncate();
};

class MParameter : public MNullaryInstruction
{
    int32_t index_;

  public:
    static const int32_t THIS_SLOT = -1;

    MParameter(int32_t index, types::StackTypeSet *types)
      : index_(index)
    {
        setResultType(MIRType_Value);
        setResultTypeSet(types);
    }

  public:
    INSTRUCTION_HEADER(Parameter)
    static MParameter *New(int32_t index, types::StackTypeSet *types);

    int32_t index() const {
        return index_;
    }
    void printOpcode(FILE *fp);

    HashNumber valueHash() const;
    bool congruentTo(MDefinition * const &ins) const;
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

    bool congruentTo(MDefinition * const &ins) const {
        return congruentIfOperandsEqual(ins);
    }

    static MCallee *New() {
        return new MCallee();
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
};

class MTableSwitch
  : public MControlInstruction
{
    // The successors of the tableswitch
    // - First successor = the default case
    // - Successor 2 and higher = the cases sorted on case index.
    Vector<MBasicBlock*, 0, IonAllocPolicy> successors_;

    // Contains the blocks/cases that still need to get build
    Vector<MBasicBlock*, 0, IonAllocPolicy> blocks_;

    MUse operand_;
    int32_t low_;
    int32_t high_;

    MTableSwitch(MDefinition *ins,
                 int32_t low, int32_t high)
      : successors_(),
        blocks_(),
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
    static MTableSwitch *New(MDefinition *ins, int32_t low, int32_t high);

    size_t numSuccessors() const {
        return successors_.length();
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
        return getSuccessor(i+1);
    }

    size_t numCases() const {
        return high() - low() + 1;
    }

    void addDefault(MBasicBlock *block) {
        JS_ASSERT(successors_.length() == 0);
        successors_.append(block);
    }

    void addCase(MBasicBlock *block) {
        JS_ASSERT(successors_.length() < (size_t)(high_ - low_ + 2));
        JS_ASSERT(successors_.length() != 0);
        successors_.append(block);
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
};

template <size_t Arity, size_t Successors>
class MAryControlInstruction : public MControlInstruction
{
    FixedArityList<MUse, Arity> operands_;
    FixedArityList<MBasicBlock *, Successors> successors_;

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }
    void setSuccessor(size_t index, MBasicBlock *successor) {
        successors_[index] = successor;
    }

    MUse *getUseFor(size_t index) {
        return &operands_[index];
    }

  public:
    MDefinition *getOperand(size_t index) const {
        return operands_[index].producer();
    }
    size_t numOperands() const {
        return Arity;
    }
    size_t numSuccessors() const {
        return Successors;
    }
    MBasicBlock *getSuccessor(size_t i) const {
        return successors_[i];
    }
    void replaceSuccessor(size_t i, MBasicBlock *succ) {
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
    static MGoto *New(MBasicBlock *target);

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
    static MTest *New(MDefinition *ins,
                      MBasicBlock *ifTrue, MBasicBlock *ifFalse);

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
    void infer(JSContext *cx);
    MDefinition *foldsTo(bool useValueNumbers);

    void markOperandCantEmulateUndefined() {
        operandMightEmulateUndefined_ = false;
    }
    bool operandMightEmulateUndefined() const {
        return operandMightEmulateUndefined_;
    }
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
    static MReturn *New(MDefinition *ins) {
        return new MReturn(ins);
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
    static MThrow *New(MDefinition *ins) {
        return new MThrow(ins);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MNewParallelArray : public MNullaryInstruction
{
    CompilerRootObject templateObject_;

    MNewParallelArray(JSObject *templateObject)
      : templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewParallelArray);

    static MNewParallelArray *New(JSObject *templateObject) {
        return new MNewParallelArray(templateObject);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    JSObject *templateObject() const {
        return templateObject_;
    }
};

// Fabricate a type set containing only the type of the specified object.
types::StackTypeSet *
MakeSingletonTypeSet(JSObject *obj);

void
MergeTypes(MIRType *ptype, types::StackTypeSet **ptypeSet,
           MIRType newType, types::StackTypeSet *newTypeSet);

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
    // Allocate space at initialization or not
    AllocatingBehaviour allocating_;

  public:
    INSTRUCTION_HEADER(NewArray)

    MNewArray(uint32_t count, JSObject *templateObject, AllocatingBehaviour allocating)
      : count_(count),
        templateObject_(templateObject),
        allocating_(allocating)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(templateObject));
    }

    uint32_t count() const {
        return count_;
    }

    JSObject *templateObject() const {
        return templateObject_;
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

    MNewObject(JSObject *templateObject)
      : templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(templateObject));
    }

  public:
    INSTRUCTION_HEADER(NewObject)

    static MNewObject *New(JSObject *templateObject) {
        return new MNewObject(templateObject);
    }

    // Returns true if the code generator should call through to the
    // VM rather than the fast path.
    bool shouldUseVM() const;

    JSObject *templateObject() const {
        return templateObject_;
    }
};

// Could be allocating either a new array or a new object.
class MParNew : public MUnaryInstruction
{
    CompilerRootObject templateObject_;

  public:
    INSTRUCTION_HEADER(ParNew);

    MParNew(MDefinition *parSlice,
            JSObject *templateObject)
      : MUnaryInstruction(parSlice),
        templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
    }

    MDefinition *parSlice() const {
        return getOperand(0);
    }

    JSObject *templateObject() const {
        return templateObject_;
    }
};

// Could be allocating either a new array or a new object.
class MParBailout : public MAryControlInstruction<0, 0>
{
  public:
    INSTRUCTION_HEADER(ParBailout);

    MParBailout()
      : MAryControlInstruction<0, 0>()
    {
        setResultType(MIRType_Undefined);
        setGuard();
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
    MInitProp(MDefinition *obj, HandlePropertyName name, MDefinition *value)
      : name_(name)
    {
        setOperand(0, obj);
        setOperand(1, value);
        setResultType(MIRType_None);
    }

  public:
    INSTRUCTION_HEADER(InitProp)

    static MInitProp *New(MDefinition *obj, HandlePropertyName name, MDefinition *value) {
        return new MInitProp(obj, name, value);
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

    static MInitElem *New(MDefinition *obj, MDefinition *id, MDefinition *value) {
        return new MInitElem(obj, id, value);
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
};

// Designates the start of call frame construction.
// Generates code to adjust the stack pointer for the argument vector.
// Argc is inferred by checking the use chain during lowering.
class MPrepareCall : public MNullaryInstruction
{
  public:
    INSTRUCTION_HEADER(PrepareCall)

    MPrepareCall()
    { }

    // Get the vector size for the upcoming call by looking at the call.
    uint32_t argc() const;

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MVariadicInstruction : public MInstruction
{
    FixedList<MUse> operands_;

  protected:
    bool init(size_t length) {
        return operands_.init(length);
    }

  public:
    // Will assert if called before initialization.
    MDefinition *getOperand(size_t index) const {
        return operands_[index].producer();
    }
    size_t numOperands() const {
        return operands_.length();
    }
    void setOperand(size_t index, MDefinition *operand) {
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }

    MUse *getUseFor(size_t index) {
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
    static const size_t PrepareCallOperandIndex  = 0;
    static const size_t FunctionOperandIndex   = 1;
    static const size_t NumNonArgumentOperands = 2;

  protected:
    // True if the call is for JSOP_NEW.
    bool construct_;
    // Monomorphic cache of single target from TI, or NULL.
    CompilerRootFunction target_;
    // Holds a target's Script alive.
    CompilerRootScript targetScript_;
    // Original value of argc from the bytecode.
    uint32_t numActualArgs_;

    bool needsArgCheck_;

    MCall(JSFunction *target, uint32_t numActualArgs, bool construct)
      : construct_(construct),
        target_(target),
        targetScript_(NULL),
        numActualArgs_(numActualArgs),
        needsArgCheck_(true)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Call)
    static MCall *New(JSFunction *target, size_t maxArgc, size_t numActualArgs, bool construct);

    void initPrepareCall(MDefinition *start) {
        JS_ASSERT(start->isPrepareCall());
        return setOperand(PrepareCallOperandIndex, start);
    }
    void initFunction(MDefinition *func) {
        JS_ASSERT(!func->isPassArg());
        return setOperand(FunctionOperandIndex, func);
    }

    bool needsArgCheck() const {
        return needsArgCheck_;
    }

    void disableArgCheck() {
        needsArgCheck_ = false;
    }

    MPrepareCall *getPrepareCall() {
        return getOperand(PrepareCallOperandIndex)->toPrepareCall();
    }
    MDefinition *getFunction() const {
        return getOperand(FunctionOperandIndex);
    }
    void replaceFunction(MInstruction *newfunc) {
        replaceOperand(FunctionOperandIndex, newfunc);
    }

    void addArg(size_t argnum, MPassArg *arg);

    MDefinition *getArg(uint32_t index) const {
        return getOperand(NumNonArgumentOperands + index);
    }

    void rootTargetScript(JSFunction *target) {
        targetScript_.setRoot(target->nonLazyScript());
    }
    bool hasRootedScript() {
        return targetScript_ != NULL;
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
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Any);
    }
};

// fun.apply(self, arguments)
class MApplyArgs
  : public MAryInstruction<3>,
    public MixPolicy<ObjectPolicy<0>, MixPolicy<IntPolicy<1>, BoxPolicy<2> > >
{
  protected:
    // Monomorphic cache of single target from TI, or NULL.
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
    static MApplyArgs *New(JSFunction *target, MDefinition *fun, MDefinition *argc,
                           MDefinition *self);

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
    New(MDefinition *scopeChain, MDefinition *name) {
        return new MGetDynamicName(scopeChain, name);
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
};

// Bailout if the input string contains 'arguments'
class MFilterArguments
  : public MAryInstruction<1>,
    public StringPolicy<0>
{
  protected:
    MFilterArguments(MDefinition *string)
    {
        setOperand(0, string);
        setGuard();
        setResultType(MIRType_None);
    }

  public:
    INSTRUCTION_HEADER(FilterArguments)

    static MFilterArguments *
    New(MDefinition *string) {
        return new MFilterArguments(string);
    }

    MDefinition *getString() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MCallDirectEval
  : public MAryInstruction<3>,
    public MixPolicy<ObjectPolicy<0>, MixPolicy<StringPolicy<1>, BoxPolicy<2> > >
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
    New(MDefinition *scopeChain, MDefinition *string, MDefinition *thisValue,
        jsbytecode *pc) {
        return new MCallDirectEval(scopeChain, string, thisValue, pc);
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

  private:
    jsbytecode *pc_;
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

    bool congruentTo(MDefinition *const &ins) const
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

        MDefinition *insLeft = ins->getOperand(0);
        MDefinition *insRight = ins->getOperand(1);
        if (isCommutative() && insLeft->valueNumber() > insRight->valueNumber()) {
            tmp = insRight;
            insRight = insLeft;
            insLeft = tmp;
        }

        return (left->valueNumber() == insLeft->valueNumber()) &&
               (right->valueNumber() == insRight->valueNumber());
    }
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

    bool congruentTo(MDefinition *const &ins) const
    {
        if (op() != ins->op())
            return false;

        if (type() != ins->type())
            return false;

        if (isEffectful() || ins->isEffectful())
            return false;

        MDefinition *first = getOperand(0);
        MDefinition *second = getOperand(1);
        MDefinition *third = getOperand(2);
        MDefinition *insFirst = ins->getOperand(0);
        MDefinition *insSecond = ins->getOperand(1);
        MDefinition *insThird = ins->getOperand(2);

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

    bool congruentTo(MDefinition *const &ins) const
    {
        if (op() != ins->op())
            return false;

        if (type() != ins->type())
            return false;

        if (isEffectful() || ins->isEffectful())
            return false;

        MDefinition *first = getOperand(0);
        MDefinition *second = getOperand(1);
        MDefinition *third = getOperand(2);
        MDefinition *fourth = getOperand(3);
        MDefinition *insFirst = ins->getOperand(0);
        MDefinition *insSecond = ins->getOperand(1);
        MDefinition *insThird = ins->getOperand(2);
        MDefinition *insFourth = ins->getOperand(3);

        return first->valueNumber() == insFirst->valueNumber() &&
               second->valueNumber() == insSecond->valueNumber() &&
               third->valueNumber() == insThird->valueNumber() &&
               fourth->valueNumber() == insFourth->valueNumber();
    }
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

        // Int32 compared as unsigneds
        Compare_UInt32,

        // Double compared to Double
        Compare_Double,

        Compare_DoubleMaybeCoerceLHS,
        Compare_DoubleMaybeCoerceRHS,

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

    MCompare(MDefinition *left, MDefinition *right, JSOp jsop)
      : MBinaryInstruction(left, right),
        compareType_(Compare_Unknown),
        jsop_(jsop),
        operandMightEmulateUndefined_(true)
    {
        setResultType(MIRType_Boolean);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Compare)
    static MCompare *New(MDefinition *left, MDefinition *right, JSOp op);
    static MCompare *NewAsmJS(MDefinition *left, MDefinition *right, JSOp op, CompareType compareType);

    bool tryFold(bool *result);
    bool evaluateConstantOperands(bool *result);
    MDefinition *foldsTo(bool useValueNumbers);

    void infer(JSContext *cx, BaselineInspector *inspector, jsbytecode *pc);
    CompareType compareType() const {
        return compareType_;
    }
    bool isDoubleComparison() const {
        return compareType() == Compare_Double ||
               compareType() == Compare_DoubleMaybeCoerceLHS ||
               compareType() == Compare_DoubleMaybeCoerceRHS;
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
    AliasSet getAliasSet() const {
        // Strict equality is never effectful.
        if (jsop_ == JSOP_STRICTEQ || jsop_ == JSOP_STRICTNE)
            return AliasSet::None();
        if (compareType_ == Compare_Unknown)
            return AliasSet::Store(AliasSet::Any);
        JS_ASSERT(compareType_ <= Compare_Value);
        return AliasSet::None();
    }

  protected:
    bool congruentTo(MDefinition *const &ins) const {
        if (!MBinaryInstruction::congruentTo(ins))
            return false;
        return compareType() == ins->toCompare()->compareType() &&
               jsop() == ins->toCompare()->jsop();
    }
};

// Takes a typed value and returns an untyped value.
class MBox : public MUnaryInstruction
{
    MBox(MDefinition *ins)
      : MUnaryInstruction(ins)
    {
        setResultType(MIRType_Value);
        if (ins->resultTypeSet()) {
            setResultTypeSet(ins->resultTypeSet());
        } else if (ins->type() != MIRType_Value) {
            types::Type ntype = ins->type() == MIRType_Object
                                ? types::Type::AnyObjectType()
                                : types::Type::PrimitiveType(ValueTypeFromMIRType(ins->type()));
            setResultTypeSet(GetIonContext()->temp->lifoAlloc()->new_<types::StackTypeSet>(ntype));
        }
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Box)
    static MBox *New(MDefinition *ins)
    {
        // Cannot box a box.
        JS_ASSERT(ins->type() != MIRType_Value);

        return new MBox(ins);
    }

    bool congruentTo(MDefinition *const &ins) const {
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
        TypeBarrier,    // Guard on the type, and act like a TypeBarrier on failure.
        TypeGuard       // Guard on the type, and deoptimize otherwise.
    };

  private:
    Mode mode_;

    MUnbox(MDefinition *ins, MIRType type, Mode mode)
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

        if (mode_ == TypeBarrier || mode_ == TypeGuard)
            setGuard();
        if (mode_ == TypeGuard)
            mode_ = Fallible;
    }

  public:
    INSTRUCTION_HEADER(Unbox)
    static MUnbox *New(MDefinition *ins, MIRType type, Mode mode)
    {
        return new MUnbox(ins, type, mode);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    Mode mode() const {
        return mode_;
    }
    MDefinition *input() const {
        return getOperand(0);
    }
    BailoutKind bailoutKind() const {
        // If infallible, no bailout should be generated.
        JS_ASSERT(fallible());
        return mode() == Fallible
               ? Bailout_Normal
               : Bailout_TypeBarrier;
    }
    bool fallible() const {
        return mode() != Infallible;
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void printOpcode(FILE *fp);
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

    static MGuardObject *New(MDefinition *ins) {
        return new MGuardObject(ins);
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

    static MGuardString *New(MDefinition *ins) {
        return new MGuardString(ins);
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

// Caller-side allocation of |this| for |new|:
// Given a templateobject, construct |this| for JSOP_NEW
class MCreateThisWithTemplate
  : public MNullaryInstruction
{
    // Template for |this|, provided by TI
    CompilerRootObject templateObject_;

    MCreateThisWithTemplate(JSObject *templateObject)
      : templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(templateObject));
    }

  public:
    INSTRUCTION_HEADER(CreateThisWithTemplate);
    static MCreateThisWithTemplate *New(JSObject *templateObject)
    {
        return new MCreateThisWithTemplate(templateObject);
    }
    JSObject *getTemplateObject() const {
        return templateObject_;
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
    static MCreateThisWithProto *New(MDefinition *callee, MDefinition *prototype)
    {
        return new MCreateThisWithProto(callee, prototype);
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
    static MCreateThis *New(MDefinition *callee)
    {
        return new MCreateThis(callee);
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
    static MCreateArgumentsObject *New(MDefinition *callObj) {
        return new MCreateArgumentsObject(callObj);
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
    static MGetArgumentsObjectArg *New(MDefinition *argsObj, size_t argno)
    {
        return new MGetArgumentsObjectArg(argsObj, argno);
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
    static MSetArgumentsObjectArg *New(MDefinition *argsObj, size_t argno, MDefinition *value)
    {
        return new MSetArgumentsObjectArg(argsObj, argno, value);
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
    static MReturnFromCtor *New(MDefinition *value, MDefinition *object)
    {
        return new MReturnFromCtor(value, object);
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

// Passes an MDefinition to an MCall. Must occur between an MPrepareCall and
// MCall. Boxes the input and stores it to the correct location on stack.
//
// Arguments are *not* simply pushed onto a call stack: they are evaluated
// left-to-right, but stored in the arg vector in C-style, right-to-left.
class MPassArg : public MUnaryInstruction
{
    int32_t argnum_;

  private:
    MPassArg(MDefinition *def)
      : MUnaryInstruction(def), argnum_(-1)
    {
        setResultType(def->type());
        setResultTypeSet(def->resultTypeSet());
    }

  public:
    INSTRUCTION_HEADER(PassArg)
    static MPassArg *New(MDefinition *def)
    {
        return new MPassArg(def);
    }

    MDefinition *getArgument() const {
        return getOperand(0);
    }

    // Set by the MCall.
    void setArgnum(uint32_t argnum) {
        argnum_ = argnum;
    }
    uint32_t getArgnum() const {
        JS_ASSERT(argnum_ >= 0);
        return (uint32_t)argnum_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void printOpcode(FILE *fp);
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
    static MToDouble *New(MDefinition *def, ConversionKind conversion = NonStringPrimitives) {
        return new MToDouble(def, conversion);
    }
    static MToDouble *NewAsmJS(MDefinition *def) {
        return new MToDouble(def);
    }

    ConversionKind conversion() const {
        return conversion_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(bool useValueNumbers);
    MDefinition *input() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        if (!ins->isToDouble() || ins->toToDouble()->conversion() != conversion())
            return false;
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange();
    bool truncate();
    bool isOperandTruncated(size_t index) const;
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
    static MAsmJSUnsignedToDouble *NewAsmJS(MDefinition *def) {
        return new MAsmJSUnsignedToDouble(def);
    }

    MDefinition *foldsTo(bool useValueNumbers);
    MDefinition *input() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Converts a primitive (either typed or untyped) to an int32. If the input is
// not primitive at runtime, a bailout occurs. If the input cannot be converted
// to an int32 without loss (i.e. "5.5" or undefined) then a bailout occurs.
class MToInt32 : public MUnaryInstruction
{
    bool canBeNegativeZero_;

    MToInt32(MDefinition *def)
      : MUnaryInstruction(def),
        canBeNegativeZero_(true)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ToInt32)
    static MToInt32 *New(MDefinition *def)
    {
        return new MToInt32(def);
    }

    MDefinition *input() const {
        return getOperand(0);
    }

    MDefinition *foldsTo(bool useValueNumbers);

    // this only has backwards information flow.
    void analyzeEdgeCasesBackward();

    bool canBeNegativeZero() {
        return canBeNegativeZero_;
    }
    void setCanBeNegativeZero(bool negativeZero) {
        canBeNegativeZero_ = negativeZero;
    }

    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange();
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
    static MTruncateToInt32 *New(MDefinition *def) {
        return new MTruncateToInt32(def);
    }
    static MTruncateToInt32 *NewAsmJS(MDefinition *def) {
        return new MTruncateToInt32(def);
    }

    MDefinition *input() const {
        return getOperand(0);
    }

    MDefinition *foldsTo(bool useValueNumbers);

    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange();
    bool isOperandTruncated(size_t index) const;
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
    static MToString *New(MDefinition *def)
    {
        return new MToString(def);
    }

    MDefinition *input() const {
        return getOperand(0);
    }

    MDefinition *foldsTo(bool useValueNumbers);

    bool congruentTo(MDefinition *const &ins) const {
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
    static MBitNot *New(MDefinition *input);
    static MBitNot *NewAsmJS(MDefinition *input);

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(bool useValueNumbers);
    void infer();

    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        if (specialization_ == MIRType_None)
            return AliasSet::Store(AliasSet::Any);
        return AliasSet::None();
    }
};

class MTypeOf
  : public MUnaryInstruction,
    public BoxInputsPolicy
{
    MIRType inputType_;

    MTypeOf(MDefinition *def, MIRType inputType)
      : MUnaryInstruction(def), inputType_(inputType)
    {
        setResultType(MIRType_String);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(TypeOf)

    static MTypeOf *New(MDefinition *def, MIRType inputType) {
        return new MTypeOf(def, inputType);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MIRType inputType() const {
        return inputType_;
    }
    MDefinition *input() const {
        return getOperand(0);
    }
    MDefinition *foldsTo(bool useValueNumbers);

    AliasSet getAliasSet() const {
        if (inputType_ <= MIRType_String)
            return AliasSet::None();

        // For objects, typeof may invoke an effectful typeof hook.
        return AliasSet::Store(AliasSet::Any);
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

    static MToId *New(MDefinition *object, MDefinition *index) {
        return new MToId(object, index);
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

    MDefinition *foldsTo(bool useValueNumbers);
    MDefinition *foldUnnecessaryBitop();
    virtual MDefinition *foldIfZero(size_t operand) = 0;
    virtual MDefinition *foldIfNegOne(size_t operand) = 0;
    virtual MDefinition *foldIfEqual()  = 0;
    virtual void infer();

    bool congruentTo(MDefinition *const &ins) const {
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
    static MBitAnd *New(MDefinition *left, MDefinition *right);
    static MBitAnd *NewAsmJS(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(operand); // 0 & x => 0;
    }
    MDefinition *foldIfNegOne(size_t operand) {
        return getOperand(1 - operand); // x & -1 => x
    }
    MDefinition *foldIfEqual() {
        return getOperand(0); // x & x => x;
    }
    void computeRange();
};

class MBitOr : public MBinaryBitwiseInstruction
{
    MBitOr(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitOr)
    static MBitOr *New(MDefinition *left, MDefinition *right);
    static MBitOr *NewAsmJS(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(1 - operand); // 0 | x => x, so if ith is 0, return (1-i)th
    }
    MDefinition *foldIfNegOne(size_t operand) {
        return getOperand(operand); // x | -1 => -1
    }
    MDefinition *foldIfEqual() {
        return getOperand(0); // x | x => x
    }
};

class MBitXor : public MBinaryBitwiseInstruction
{
    MBitXor(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitXor)
    static MBitXor *New(MDefinition *left, MDefinition *right);
    static MBitXor *NewAsmJS(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(1 - operand); // 0 ^ x => x
    }
    MDefinition *foldIfNegOne(size_t operand) {
        return this;
    }
    MDefinition *foldIfEqual() {
        return this;
    }
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
    virtual void infer();
};

class MLsh : public MShiftInstruction
{
    MLsh(MDefinition *left, MDefinition *right)
      : MShiftInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(Lsh)
    static MLsh *New(MDefinition *left, MDefinition *right);
    static MLsh *NewAsmJS(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 << x => 0
        // x << 0 => x
        return getOperand(0);
    }

    void computeRange();
};

class MRsh : public MShiftInstruction
{
    MRsh(MDefinition *left, MDefinition *right)
      : MShiftInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(Rsh)
    static MRsh *New(MDefinition *left, MDefinition *right);
    static MRsh *NewAsmJS(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 >> x => 0
        // x >> 0 => x
        return getOperand(0);
    }
    void computeRange();
};

class MUrsh : public MShiftInstruction
{
    bool canOverflow_;

    MUrsh(MDefinition *left, MDefinition *right)
      : MShiftInstruction(left, right),
        canOverflow_(true)
    { }

  public:
    INSTRUCTION_HEADER(Ursh)
    static MUrsh *New(MDefinition *left, MDefinition *right);
    static MUrsh *NewAsmJS(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 >>> x => 0
        if (operand == 0)
            return getOperand(0);

        return this;
    }

    void infer();

    bool canOverflow() {
        // solution is only negative when lhs < 0 and rhs & 0x1f == 0
        MDefinition *lhs = getOperand(0);
        MDefinition *rhs = getOperand(1);

        if (lhs->isConstant()) {
            Value lhsv = lhs->toConstant()->value();
            if (lhsv.isInt32() && lhsv.toInt32() >= 0)
                return false;
        }

        if (rhs->isConstant()) {
            Value rhsv = rhs->toConstant()->value();
            if (rhsv.isInt32() && rhsv.toInt32() % 32 != 0)
                return false;
        }

        return canOverflow_;
    }

    bool fallible() {
        return canOverflow();
    }
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

    MDefinition *foldsTo(bool useValueNumbers);

    virtual double getIdentity() = 0;

    void infer(BaselineInspector *inspector,
               jsbytecode *pc, bool overflowed);

    void setInt32() {
        specialization_ = MIRType_Int32;
        setResultType(MIRType_Int32);
    }

    bool congruentTo(MDefinition *const &ins) const {
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
    static MMinMax *New(MDefinition *left, MDefinition *right, MIRType type, bool isMax) {
        return new MMinMax(left, right, type, isMax);
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
    bool congruentTo(MDefinition *const &ins) const {
        if (!ins->isMinMax())
            return false;
        if (isMax() != ins->toMinMax()->isMax())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
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
        JS_ASSERT(type == MIRType_Double || type == MIRType_Int32);
        setResultType(type);
        setMovable();
        specialization_ = type;
    }

  public:
    INSTRUCTION_HEADER(Abs)
    static MAbs *New(MDefinition *num, MIRType type) {
        return new MAbs(num, type);
    }
    static MAbs *NewAsmJS(MDefinition *num, MIRType type) {
        MAbs *ins = new MAbs(num, type);
        ins->implicitTruncate_ = true;
        return ins;
    }
    MDefinition *num() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    bool fallible() const;

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange();
};

// Inline implementation of Math.sqrt().
class MSqrt
  : public MUnaryInstruction,
    public DoublePolicy<0>
{
    MSqrt(MDefinition *num)
      : MUnaryInstruction(num)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(Sqrt)
    static MSqrt *New(MDefinition *num) {
        return new MSqrt(num);
    }
    MDefinition *num() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
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
    static MPow *New(MDefinition *input, MDefinition *power, MIRType powerType) {
        return new MPow(input, power, powerType);
    }

    MDefinition *input() const {
        return lhs();
    }
    MDefinition *power() const {
        return rhs();
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

// Inline implementation of Math.pow(x, 0.5), which subtly differs from Math.sqrt(x).
class MPowHalf
  : public MUnaryInstruction,
    public DoublePolicy<0>
{
    MPowHalf(MDefinition *input)
      : MUnaryInstruction(input)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(PowHalf)
    static MPowHalf *New(MDefinition *input) {
        return new MPowHalf(input);
    }
    MDefinition *input() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
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
    static MRandom *New() {
        return new MRandom;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MMathFunction
  : public MUnaryInstruction,
    public DoublePolicy<0>
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
        ATan
    };

  private:
    Function function_;
    MathCache *cache_;

    MMathFunction(MDefinition *input, Function function, MathCache *cache)
      : MUnaryInstruction(input), function_(function), cache_(cache)
    {
        setResultType(MIRType_Double);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(MathFunction)
    static MMathFunction *New(MDefinition *input, Function function, MathCache *cache) {
        return new MMathFunction(input, function, cache);
    }
    Function function() const {
        return function_;
    }
    MathCache *cache() const {
        return cache_;
    }
    MDefinition *input() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *const &ins) const {
        if (!ins->isMathFunction())
            return false;
        if (ins->toMathFunction()->function() != function())
            return false;
        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
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
    static MAdd *New(MDefinition *left, MDefinition *right) {
        return new MAdd(left, right);
    }

    static MAdd *NewAsmJS(MDefinition *left, MDefinition *right, MIRType type) {
        MAdd *add = new MAdd(left, right);
        add->specialization_ = type;
        add->setResultType(type);
        if (type == MIRType_Int32) {
            add->setTruncated(true);
            add->setCommutative();
        }
        return add;
    }
    void analyzeTruncateBackward();

    double getIdentity() {
        return 0;
    }

    bool fallible();
    void computeRange();
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
    static MSub *New(MDefinition *left, MDefinition *right) {
        return new MSub(left, right);
    }
    static MSub *NewAsmJS(MDefinition *left, MDefinition *right, MIRType type) {
        MSub *sub = new MSub(left, right);
        sub->specialization_ = type;
        sub->setResultType(type);
        if (type == MIRType_Int32)
            sub->setTruncated(true);
        return sub;
    }

    double getIdentity() {
        return 0;
    }

    bool fallible();
    void computeRange();
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
    static MMul *New(MDefinition *left, MDefinition *right) {
        return new MMul(left, right, MIRType_Value, MMul::Normal);
    }
    static MMul *New(MDefinition *left, MDefinition *right, MIRType type, Mode mode = Normal) {
        return new MMul(left, right, type, mode);
    }

    MDefinition *foldsTo(bool useValueNumbers);
    void analyzeEdgeCasesForward();
    void analyzeEdgeCasesBackward();

    double getIdentity() {
        return 1;
    }

    bool canOverflow();

    bool canBeNegativeZero() {
        return canBeNegativeZero_;
    }
    void setCanBeNegativeZero(bool negativeZero) {
        canBeNegativeZero_ = negativeZero;
    }

    bool updateForReplacement(MDefinition *ins);

    bool fallible() {
        return canBeNegativeZero_ || canOverflow();
    }

    void computeRange();
    bool truncate();
    bool isOperandTruncated(size_t index) const;

    Mode mode() { return mode_; }
};

class MDiv : public MBinaryArithInstruction
{
    bool canBeNegativeZero_;
    bool canBeNegativeOverflow_;
    bool canBeDivideByZero_;

    MDiv(MDefinition *left, MDefinition *right, MIRType type)
      : MBinaryArithInstruction(left, right),
        canBeNegativeZero_(true),
        canBeNegativeOverflow_(true),
        canBeDivideByZero_(true)
    {
        if (type != MIRType_Value)
            specialization_ = type;
        setResultType(type);
    }

  public:
    INSTRUCTION_HEADER(Div)
    static MDiv *New(MDefinition *left, MDefinition *right) {
        return new MDiv(left, right, MIRType_Value);
    }
    static MDiv *New(MDefinition *left, MDefinition *right, MIRType type) {
        return new MDiv(left, right, type);
    }
    static MDiv *NewAsmJS(MDefinition *left, MDefinition *right, MIRType type) {
        MDiv *div = new MDiv(left, right, type);
        if (type == MIRType_Int32)
            div->setTruncated(true);
        return div;
    }

    MDefinition *foldsTo(bool useValueNumbers);
    void analyzeEdgeCasesForward();
    void analyzeEdgeCasesBackward();

    double getIdentity() {
        JS_NOT_REACHED("not used");
        return 1;
    }

    bool canBeNegativeZero() {
        return canBeNegativeZero_;
    }
    void setCanBeNegativeZero(bool negativeZero) {
        canBeNegativeZero_ = negativeZero;
    }

    bool canBeNegativeOverflow() {
        return canBeNegativeOverflow_;
    }

    bool canBeDivideByZero() {
        return canBeDivideByZero_;
    }

    bool fallible();
    bool truncate();
};

class MMod : public MBinaryArithInstruction
{
    MMod(MDefinition *left, MDefinition *right, MIRType type)
      : MBinaryArithInstruction(left, right)
    {
        if (type != MIRType_Value)
            specialization_ = type;
        setResultType(type);
    }

  public:
    INSTRUCTION_HEADER(Mod)
    static MMod *New(MDefinition *left, MDefinition *right) {
        return new MMod(left, right, MIRType_Value);
    }
    static MMod *NewAsmJS(MDefinition *left, MDefinition *right, MIRType type) {
        MMod *mod = new MMod(left, right, type);
        if (type == MIRType_Int32)
            mod->setTruncated(true);
        return mod;
    }

    MDefinition *foldsTo(bool useValueNumbers);

    double getIdentity() {
        JS_NOT_REACHED("not used");
        return 1;
    }

    bool fallible();

    void computeRange();
    bool truncate();
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
    static MConcat *New(MDefinition *left, MDefinition *right) {
        return new MConcat(left, right);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *const &ins) const {
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

    static MCharCodeAt *New(MDefinition *str, MDefinition *index) {
        return new MCharCodeAt(str, index);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    virtual AliasSet getAliasSet() const {
        // Strings are immutable, so there is no implicit dependency.
        return AliasSet::None();
    }

    void computeRange();
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

    static MFromCharCode *New(MDefinition *code) {
        return new MFromCharCode(code);
    }

    virtual AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MPhi : public MDefinition, public InlineForwardListNode<MPhi>
{
    js::Vector<MUse, 2, IonAllocPolicy> inputs_;

    uint32_t slot_;
    bool hasBackedgeType_;
    bool triedToSpecialize_;
    bool isIterator_;

#if DEBUG
    bool specialized_;
    uint32_t capacity_;
#endif

    MPhi(uint32_t slot)
      : slot_(slot),
        hasBackedgeType_(false),
        triedToSpecialize_(false),
        isIterator_(false)
#if DEBUG
        , specialized_(false)
        , capacity_(0)
#endif
    {
        setResultType(MIRType_Value);
    }

  protected:
    MUse *getUseFor(size_t index) {
        return &inputs_[index];
    }

  public:
    INSTRUCTION_HEADER(Phi)
    static MPhi *New(uint32_t slot);

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
    void addBackedgeType(MIRType type, types::StackTypeSet *typeSet);

    // Initializes the operands vector to the given capacity,
    // permitting use of addInput() instead of addInputSlow().
    bool reserveLength(size_t length);

    // Use only if capacity has been reserved by reserveLength
    void addInput(MDefinition *ins);

    // Appends a new input to the input vector. May call realloc().
    // Prefer reserveLength() and addInput() instead, where possible.
    bool addInputSlow(MDefinition *ins, bool *ptypeChange = NULL);

    MDefinition *foldsTo(bool useValueNumbers);

    bool congruentTo(MDefinition * const &ins) const;

    bool isIterator() const {
        return isIterator_;
    }
    void setIterator() {
        isIterator_ = true;
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange();

    MDefinition *operandIfRedundant() {
        // If this phi is redundant (e.g., phi(a,a) or b=phi(a,this)),
        // returns the operand that it will always be equal to (a, in
        // those two cases).
        MDefinition *first = getOperand(0);
        for (size_t i = 1; i < numOperands(); i++) {
            if (getOperand(i) != first && getOperand(i) != this)
                return NULL;
        }
        return first;
    }
};

// The goal of a Beta node is to split a def at a conditionally taken
// branch, so that uses dominated by it have a different name.
class MBeta : public MUnaryInstruction
{
  private:
    const Range *comparison_;
    MDefinition *val_;
    MBeta(MDefinition *val, const Range *comp)
        : MUnaryInstruction(val),
          comparison_(comp),
          val_(val)
    {
        setResultType(val->type());
        setResultTypeSet(val->resultTypeSet());
    }

  public:
    INSTRUCTION_HEADER(Beta)
    void printOpcode(FILE *fp);
    static MBeta *New(MDefinition *val, const Range *comp)
    {
        return new MBeta(val, comp);
    }

    AliasSet getAliasSet() const {
        return AliasSet::None();
    }

    void computeRange();
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
    static MOsrValue *New(MOsrEntry *entry, ptrdiff_t frameOffset) {
        return new MOsrValue(entry, frameOffset);
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
    static MOsrScopeChain *New(MOsrEntry *entry) {
        return new MOsrScopeChain(entry);
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
};

// Check the current frame for over-recursion past the global stack limit.
// Uses the per-thread recursion limit.
class MParCheckOverRecursed : public MUnaryInstruction
{
  public:
    INSTRUCTION_HEADER(ParCheckOverRecursed);

    MParCheckOverRecursed(MDefinition *parForkJoinSlice)
      : MUnaryInstruction(parForkJoinSlice)
    {
        setResultType(MIRType_None);
        setGuard();
        setMovable();
    }

    MDefinition *parSlice() const {
        return getOperand(0);
    }
};

// Check for an interrupt (or rendezvous) in parallel mode.
class MParCheckInterrupt : public MUnaryInstruction
{
  public:
    INSTRUCTION_HEADER(ParCheckInterrupt);

    MParCheckInterrupt(MDefinition *parForkJoinSlice)
      : MUnaryInstruction(parForkJoinSlice)
    {
        setResultType(MIRType_None);
        setGuard();
        setMovable();
    }

    MDefinition *parSlice() const {
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

    static MInterruptCheck *New() {
        return new MInterruptCheck();
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

    static MDefVar *New(PropertyName *name, unsigned attrs, MDefinition *scopeChain) {
        return new MDefVar(name, attrs, scopeChain);
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

};

class MDefFun : public MUnaryInstruction
{
    CompilerRootFunction fun_;

  private:
    MDefFun(HandleFunction fun, MDefinition *scopeChain)
      : MUnaryInstruction(scopeChain),
        fun_(fun)
    {}

  public:
    INSTRUCTION_HEADER(DefFun)

    static MDefFun *New(HandleFunction fun, MDefinition *scopeChain) {
        return new MDefFun(fun, scopeChain);
    }

    JSFunction *fun() const {
        return fun_;
    }
    MDefinition *scopeChain() const {
        return getOperand(0);
    }
};

class MRegExp : public MNullaryInstruction
{
    CompilerRoot<RegExpObject *> source_;
    CompilerRootObject prototype_;

    MRegExp(RegExpObject *source, JSObject *prototype)
      : source_(source),
        prototype_(prototype)
    {
        setResultType(MIRType_Object);

        JS_ASSERT(source->getProto() == prototype);
        setResultTypeSet(MakeSingletonTypeSet(source));
    }

  public:
    INSTRUCTION_HEADER(RegExp)

    static MRegExp *New(RegExpObject *source, JSObject *prototype) {
        return new MRegExp(source, prototype);
    }

    RegExpObject *source() const {
        return source_;
    }
    JSObject *getRegExpPrototype() const {
        return prototype_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
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

    static MRegExpTest *New(MDefinition *regexp, MDefinition *string) {
        return new MRegExpTest(regexp, string);
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
};

class MLambda
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRootFunction fun_;

    MLambda(MDefinition *scopeChain, JSFunction *fun)
      : MUnaryInstruction(scopeChain), fun_(fun)
    {
        setResultType(MIRType_Object);
        if (!fun->hasSingletonType() && !types::UseNewTypeForClone(fun))
            setResultTypeSet(MakeSingletonTypeSet(fun));
    }

  public:
    INSTRUCTION_HEADER(Lambda)

    static MLambda *New(MDefinition *scopeChain, JSFunction *fun) {
        return new MLambda(scopeChain, fun);
    }
    MDefinition *scopeChain() const {
        return getOperand(0);
    }
    JSFunction *fun() const {
        return fun_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
};

class MParLambda
  : public MBinaryInstruction,
    public SingleObjectPolicy
{
    CompilerRootFunction fun_;

    MParLambda(MDefinition *parSlice,
               MDefinition *scopeChain, JSFunction *fun)
      : MBinaryInstruction(parSlice, scopeChain), fun_(fun)
    {
        JS_ASSERT(!fun->hasSingletonType());
        JS_ASSERT(!types::UseNewTypeForClone(fun));
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(fun));
    }

  public:
    INSTRUCTION_HEADER(ParLambda);

    static MParLambda *New(MDefinition *parSlice,
                           MDefinition *scopeChain, JSFunction *fun) {
        return new MParLambda(parSlice, scopeChain, fun);
    }

    static MParLambda *New(MDefinition *parSlice,
                           MLambda *originalInstruction) {
        return New(parSlice,
                   originalInstruction->scopeChain(),
                   originalInstruction->fun());
    }

    MDefinition *parSlice() const {
        return getOperand(0);
    }

    MDefinition *scopeChain() const {
        return getOperand(1);
    }

    JSFunction *fun() const {
        return fun_;
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

    static MImplicitThis *New(MDefinition *callee) {
        return new MImplicitThis(callee);
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

    static MSlots *New(MDefinition *object) {
        return new MSlots(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
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

    static MElements *New(MDefinition *object) {
        return new MElements(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
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
    static MConstantElements *New(void *v) {
        return new MConstantElements(v);
    }

    void *value() const {
        return value_;
    }

    void printOpcode(FILE *fp);

    HashNumber valueHash() const {
        return (HashNumber)(size_t) value_;
    }

    bool congruentTo(MDefinition * const &ins) const {
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

    static MConvertElementsToDoubles *New(MDefinition *elements) {
        return new MConvertElementsToDoubles(elements);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
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

// Load a dense array's initialized length from an elements vector.
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

    static MInitializedLength *New(MDefinition *elements) {
        return new MInitializedLength(elements);
    }

    MDefinition *elements() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
    }
};

// Set a dense array's initialized length to an elements vector.
class MSetInitializedLength
  : public MAryInstruction<2>
{
    MSetInitializedLength(MDefinition *elements, MDefinition *index)
    {
        setOperand(0, elements);
        setOperand(1, index);
    }

  public:
    INSTRUCTION_HEADER(SetInitializedLength)

    static MSetInitializedLength *New(MDefinition *elements, MDefinition *index) {
        return new MSetInitializedLength(elements, index);
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

// Load a dense array's initialized length from an elements vector.
class MArrayLength
  : public MUnaryInstruction
{
  public:
    MArrayLength(MDefinition *elements)
      : MUnaryInstruction(elements)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

    INSTRUCTION_HEADER(ArrayLength)

    MDefinition *elements() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::ObjectFields);
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

    static MTypedArrayLength *New(MDefinition *obj) {
        return new MTypedArrayLength(obj);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // The typed array |length| property is immutable, so there is no
        // implicit dependency.
        return AliasSet::None();
    }
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

    static MTypedArrayElements *New(MDefinition *object) {
        return new MTypedArrayElements(object);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    MDefinition *object() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
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

  public:
    MNot(MDefinition *input)
      : MUnaryInstruction(input),
        operandMightEmulateUndefined_(true)
    {
        setResultType(MIRType_Boolean);
        setMovable();
    }

    static MNot *New(MDefinition *elements) {
        return new MNot(elements);
    }
    static MNot *NewAsmJS(MDefinition *elements) {
        MNot *ins = new MNot(elements);
        ins->setResultType(MIRType_Int32);
        return ins;
    }

    INSTRUCTION_HEADER(Not);

    void infer(JSContext *cx);
    MDefinition *foldsTo(bool useValueNumbers);

    void markOperandCantEmulateUndefined() {
        operandMightEmulateUndefined_ = false;
    }
    bool operandMightEmulateUndefined() const {
        return operandMightEmulateUndefined_;
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

    static MBoundsCheck *New(MDefinition *index, MDefinition *length) {
        return new MBoundsCheck(index, length);
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
    bool congruentTo(MDefinition * const &ins) const {
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
};

// Bailout if index < minimum.
class MBoundsCheckLower
  : public MUnaryInstruction
{
    int32_t minimum_;

    MBoundsCheckLower(MDefinition *index)
      : MUnaryInstruction(index), minimum_(0)
    {
        setGuard();
        setMovable();
        JS_ASSERT(index->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(BoundsCheckLower)

    static MBoundsCheckLower *New(MDefinition *index) {
        return new MBoundsCheckLower(index);
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
    bool fallible();
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

    static MLoadElement *New(MDefinition *elements, MDefinition *index,
                             bool needsHoleCheck, bool loadDoubles) {
        return new MLoadElement(elements, index, needsHoleCheck, loadDoubles);
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
    bool needsHoleCheck_;

    MLoadElementHole(MDefinition *elements, MDefinition *index, MDefinition *initLength, bool needsHoleCheck)
      : MTernaryInstruction(elements, index, initLength),
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

    static MLoadElementHole *New(MDefinition *elements, MDefinition *index,
                                 MDefinition *initLength, bool needsHoleCheck) {
        return new MLoadElementHole(elements, index, initLength, needsHoleCheck);
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
    bool needsHoleCheck() const {
        return needsHoleCheck_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Load(AliasSet::Element);
    }
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
    public SingleObjectPolicy
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

    static MStoreElement *New(MDefinition *elements, MDefinition *index, MDefinition *value,
                              bool needsHoleCheck) {
        return new MStoreElement(elements, index, value, needsHoleCheck);
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
    public SingleObjectPolicy
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

    static MStoreElementHole *New(MDefinition *object, MDefinition *elements,
                                  MDefinition *index, MDefinition *value) {
        return new MStoreElementHole(object, elements, index, value);
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

    static MArrayPopShift *New(MDefinition *object, Mode mode, bool needsHoleCheck,
                               bool maybeUndefined) {
        return new MArrayPopShift(object, mode, needsHoleCheck, maybeUndefined);
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
    public SingleObjectPolicy
{
    MArrayPush(MDefinition *object, MDefinition *value)
      : MBinaryInstruction(object, value)
    {
        setResultType(MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(ArrayPush)

    static MArrayPush *New(MDefinition *object, MDefinition *value) {
        return new MArrayPush(object, value);
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
};

// Array.prototype.concat on two dense arrays.
class MArrayConcat
  : public MBinaryInstruction,
    public MixPolicy<ObjectPolicy<0>, ObjectPolicy<1> >
{
    CompilerRootObject templateObj_;

    MArrayConcat(MDefinition *lhs, MDefinition *rhs, HandleObject templateObj)
      : MBinaryInstruction(lhs, rhs),
        templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(ArrayConcat)

    static MArrayConcat *New(MDefinition *lhs, MDefinition *rhs, HandleObject templateObj) {
        return new MArrayConcat(lhs, rhs, templateObj);
    }

    JSObject *templateObj() const {
        return templateObj_;
    }
    TypePolicy *typePolicy() {
        return this;
    }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::Element | AliasSet::ObjectFields);
    }
};

class MLoadTypedArrayElement
  : public MBinaryInstruction
{
    int arrayType_;

    MLoadTypedArrayElement(MDefinition *elements, MDefinition *index, int arrayType)
      : MBinaryInstruction(elements, index), arrayType_(arrayType)
    {
        setResultType(MIRType_Value);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(arrayType >= 0 && arrayType < TypedArray::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(LoadTypedArrayElement)

    static MLoadTypedArrayElement *New(MDefinition *elements, MDefinition *index, int arrayType) {
        return new MLoadTypedArrayElement(elements, index, arrayType);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool fallible() const {
        // Bailout if the result does not fit in an int32.
        return arrayType_ == TypedArray::TYPE_UINT32 && type() == MIRType_Int32;
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
        JS_ASSERT(arrayType >= 0 && arrayType < TypedArray::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(LoadTypedArrayElementHole)

    static MLoadTypedArrayElementHole *New(MDefinition *object, MDefinition *index, int arrayType, bool allowDouble) {
        return new MLoadTypedArrayElementHole(object, index, arrayType, allowDouble);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool allowDouble() const {
        return allowDouble_;
    }
    bool fallible() const {
        return arrayType_ == TypedArray::TYPE_UINT32 && !allowDouble_;
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
        // Out-of-bounds accesses are handled using a VM call, this may
        // invoke getters on the prototype chain.
        return AliasSet::Store(AliasSet::Any);
    }
};

// Load a value fallibly or infallibly from a statically known typed array.
class MLoadTypedArrayElementStatic : public MUnaryInstruction
{
    MLoadTypedArrayElementStatic(JSObject *typedArray, MDefinition *ptr)
      : MUnaryInstruction(ptr), typedArray_(typedArray), fallible_(true)
    {
        int type = TypedArray::type(typedArray_);
        if (type == TypedArray::TYPE_FLOAT32 || type == TypedArray::TYPE_FLOAT64)
            setResultType(MIRType_Double);
        else
            setResultType(MIRType_Int32);
    }

    CompilerRootObject typedArray_;
    bool fallible_;

  public:
    INSTRUCTION_HEADER(LoadTypedArrayElementStatic);

    static MLoadTypedArrayElementStatic *New(JSObject *typedArray, MDefinition *ptr) {
        return new MLoadTypedArrayElementStatic(typedArray, ptr);
    }

    ArrayBufferView::ViewType viewType() const { return JS_GetArrayBufferViewType(typedArray_); }
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

    void computeRange();
    bool truncate();
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
        JS_ASSERT(arrayType >= 0 && arrayType < TypedArray::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(StoreTypedArrayElement)

    static MStoreTypedArrayElement *New(MDefinition *elements, MDefinition *index, MDefinition *value,
                                        int arrayType) {
        return new MStoreTypedArrayElement(elements, index, value, arrayType);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool isByteArray() const {
        return (arrayType_ == TypedArray::TYPE_INT8 ||
                arrayType_ == TypedArray::TYPE_UINT8 ||
                arrayType_ == TypedArray::TYPE_UINT8_CLAMPED);
    }
    bool isFloatArray() const {
        return (arrayType_ == TypedArray::TYPE_FLOAT32 ||
                arrayType_ == TypedArray::TYPE_FLOAT64);
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
        JS_ASSERT(arrayType >= 0 && arrayType < TypedArray::TYPE_MAX);
    }

  public:
    INSTRUCTION_HEADER(StoreTypedArrayElementHole)

    static MStoreTypedArrayElementHole *New(MDefinition *elements, MDefinition *length,
                                            MDefinition *index, MDefinition *value, int arrayType)
    {
        return new MStoreTypedArrayElementHole(elements, length, index, value, arrayType);
    }

    int arrayType() const {
        return arrayType_;
    }
    bool isByteArray() const {
        return (arrayType_ == TypedArray::TYPE_INT8 ||
                arrayType_ == TypedArray::TYPE_UINT8 ||
                arrayType_ == TypedArray::TYPE_UINT8_CLAMPED);
    }
    bool isFloatArray() const {
        return (arrayType_ == TypedArray::TYPE_FLOAT32 ||
                arrayType_ == TypedArray::TYPE_FLOAT64);
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
};

// Store a value infallibly to a statically known typed array.
class MStoreTypedArrayElementStatic :
    public MBinaryInstruction
  , public StoreTypedArrayElementStaticPolicy
{
    MStoreTypedArrayElementStatic(JSObject *typedArray, MDefinition *ptr, MDefinition *v)
      : MBinaryInstruction(ptr, v), typedArray_(typedArray)
    {}

    CompilerRootObject typedArray_;

  public:
    INSTRUCTION_HEADER(StoreTypedArrayElementStatic);

    static MStoreTypedArrayElementStatic *New(JSObject *typedArray, MDefinition *ptr, MDefinition *v) {
        return new MStoreTypedArrayElementStatic(typedArray, ptr, v);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    ArrayBufferView::ViewType viewType() const { return JS_GetArrayBufferViewType(typedArray_); }
    void *base() const;
    size_t length() const;

    MDefinition *ptr() const { return getOperand(0); }
    MDefinition *value() const { return getOperand(1); }
    AliasSet getAliasSet() const {
        return AliasSet::Store(AliasSet::TypedArrayElement);
    }
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

    static MEffectiveAddress *New(MDefinition *base, MDefinition *index, Scale s, int32_t d) {
        return new MEffectiveAddress(base, index, s, d);
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

    static MClampToUint8 *New(MDefinition *input) {
        return new MClampToUint8(input);
    }

    MDefinition *foldsTo(bool useValueNumbers);

    MDefinition *input() const {
        return getOperand(0);
    }
    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    void computeRange();
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

    static MLoadFixedSlot *New(MDefinition *obj, size_t slot) {
        return new MLoadFixedSlot(obj, slot);
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
    bool congruentTo(MDefinition * const &ins) const {
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
    public SingleObjectPolicy
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

    static MStoreFixedSlot *New(MDefinition *obj, size_t slot, MDefinition *rval) {
        return new MStoreFixedSlot(obj, rval, slot, false);
    }
    static MStoreFixedSlot *NewBarriered(MDefinition *obj, size_t slot, MDefinition *rval) {
        return new MStoreFixedSlot(obj, rval, slot, true);
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
    InlinePropertyTable(jsbytecode *pc)
      : pc_(pc), priorResumePoint_(NULL), entries_()
    { }

    void setPriorResumePoint(MResumePoint *resumePoint) {
        JS_ASSERT(priorResumePoint_ == NULL);
        priorResumePoint_ = resumePoint;
    }

    MResumePoint *priorResumePoint() const {
        return priorResumePoint_;
    }

    jsbytecode *pc() const {
        return pc_;
    }

    bool addEntry(types::TypeObject *typeObj, JSFunction *func) {
        return entries_.append(new Entry(typeObj, func));
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
    types::StackTypeSet *buildTypeSetForFunction(JSFunction *func) const;

    // Remove targets that vetoed inlining from the InlinePropertyTable.
    void trimTo(AutoObjectVector &targets, Vector<bool> &choiceSet);

    // Ensure that the InlinePropertyTable's domain is a subset of |targets|.
    void trimToAndMaybePatchTargets(AutoObjectVector &targets, AutoObjectVector &originals);
};

class MGetPropertyCache
  : public MUnaryInstruction,
    public SingleObjectPolicy
{
    CompilerRootPropertyName name_;
    bool idempotent_;
    bool allowGetters_;

    InlinePropertyTable *inlinePropertyTable_;

    MGetPropertyCache(MDefinition *obj, HandlePropertyName name)
      : MUnaryInstruction(obj),
        name_(name),
        idempotent_(false),
        allowGetters_(false),
        inlinePropertyTable_(NULL)
    {
        setResultType(MIRType_Value);

        // The cache will invalidate if there are objects with e.g. lookup or
        // resolve hooks on the proto chain. setGuard ensures this check is not
        // eliminated.
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(GetPropertyCache)

    static MGetPropertyCache *New(MDefinition *obj, HandlePropertyName name) {
        return new MGetPropertyCache(obj, name);
    }

    InlinePropertyTable *initInlinePropertyTable(jsbytecode *pc) {
        JS_ASSERT(inlinePropertyTable_ == NULL);
        inlinePropertyTable_ = new InlinePropertyTable(pc);
        return inlinePropertyTable_;
    }

    void clearInlinePropertyTable() {
        inlinePropertyTable_ = NULL;
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
    void setAllowGetters() {
        allowGetters_ = true;
    }
    TypePolicy *typePolicy() { return this; }

    bool congruentTo(MDefinition * const &ins) const {
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

    MGetPropertyPolymorphic(MDefinition *obj, HandlePropertyName name)
      : MUnaryInstruction(obj),
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

    static MGetPropertyPolymorphic *New(MDefinition *obj, HandlePropertyName name) {
        return new MGetPropertyPolymorphic(obj, name);
    }

    bool congruentTo(MDefinition *const &ins) const {
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

    MSetPropertyPolymorphic(MDefinition *obj, MDefinition *value)
      : MBinaryInstruction(obj, value),
        needsBarrier_(false)
    {
    }

  public:
    INSTRUCTION_HEADER(SetPropertyPolymorphic)

    static MSetPropertyPolymorphic *New(MDefinition *obj, MDefinition *value) {
        return new MSetPropertyPolymorphic(obj, value);
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
    MDispatchInstruction(MDefinition *input)
      : map_(), fallback_(NULL)
    {
        setOperand(0, input);
    }

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index == 0);
        operand_.set(operand, this, 0);
        operand->addUse(&operand_);
    }
    MUse *getUseFor(size_t index) {
        JS_ASSERT(index == 0);
        return &operand_;
    }
    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index == 0);
        return operand_.producer();
    }
    size_t numOperands() const {
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
    size_t numSuccessors() const {
        return map_.length() + (fallback_ ? 1 : 0);
    }
    void replaceSuccessor(size_t i, MBasicBlock *successor) {
        setSuccessor(i, successor);
    }
    MBasicBlock *getSuccessor(size_t i) const {
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

    MTypeObjectDispatch(MDefinition *input, InlinePropertyTable *table)
      : MDispatchInstruction(input),
        inlinePropertyTable_(table)
    { }

  public:
    INSTRUCTION_HEADER(TypeObjectDispatch)

    static MTypeObjectDispatch *New(MDefinition *ins, InlinePropertyTable *table) {
        return new MTypeObjectDispatch(ins, table);
    }

    InlinePropertyTable *propTable() const {
        return inlinePropertyTable_;
    }
};

// Polymorphic dispatch for inlining, keyed off incoming JSFunction*.
class MFunctionDispatch : public MDispatchInstruction
{
    MFunctionDispatch(MDefinition *input)
      : MDispatchInstruction(input)
    { }

  public:
    INSTRUCTION_HEADER(FunctionDispatch)

    static MFunctionDispatch *New(MDefinition *ins) {
        return new MFunctionDispatch(ins);
    }
};

// Represents a polymorphic dispatch to one or more functions.
class MPolyInlineDispatch : public MControlInstruction, public SingleObjectPolicy
{
    // A table to map JSFunctions to the blocks that execute them.
    struct Entry {
        MConstant *funcConst;
        MBasicBlock *block;
        Entry(MConstant *funcConst, MBasicBlock *block)
            : funcConst(funcConst), block(block) {}
    };
    Vector<Entry, 4, IonAllocPolicy> dispatchTable_;

    MUse operand_;
    InlinePropertyTable *inlinePropertyTable_;
    MBasicBlock *fallbackPrepBlock_;
    MBasicBlock *fallbackMidBlock_;
    MBasicBlock *fallbackEndBlock_;

    MPolyInlineDispatch(MDefinition *ins)
      : dispatchTable_(),
        inlinePropertyTable_(NULL),
        fallbackPrepBlock_(NULL),
        fallbackMidBlock_(NULL),
        fallbackEndBlock_(NULL)
    {
        setOperand(0, ins);
    }

    MPolyInlineDispatch(MDefinition *ins, InlinePropertyTable *inlinePropertyTable,
                        MBasicBlock *fallbackPrepBlock,
                        MBasicBlock *fallbackMidBlock,
                        MBasicBlock *fallbackEndBlock)
      : dispatchTable_(),
        inlinePropertyTable_(inlinePropertyTable),
        fallbackPrepBlock_(fallbackPrepBlock),
        fallbackMidBlock_(fallbackMidBlock),
        fallbackEndBlock_(fallbackEndBlock)
    {
        setOperand(0, ins);
    }

  protected:
    virtual void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index == 0);
        operand_.set(operand, this, index);
        operand->addUse(&operand_);
    }

    MUse *getUseFor(size_t index) {
        JS_ASSERT(index == 0);
        return &operand_;
    }

    void setSuccessor(size_t i, MBasicBlock *successor) {
        JS_ASSERT(i < numSuccessors());
        if (inlinePropertyTable_ && (i == numSuccessors() - 1))
            fallbackPrepBlock_ = successor;
        else
            dispatchTable_[i].block = successor;
    }

  public:
    INSTRUCTION_HEADER(PolyInlineDispatch)

    virtual MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index == 0);
        return operand_.producer();
    }

    virtual size_t numOperands() const {
        return 1;
    }
    virtual size_t numSuccessors() const {
        return dispatchTable_.length() + (inlinePropertyTable_ ? 1 : 0);
    }

    virtual void replaceSuccessor(size_t i, MBasicBlock *successor) {
        setSuccessor(i, successor);
    }

    MBasicBlock *getSuccessor(size_t i) const {
        JS_ASSERT(i < numSuccessors());
        if (inlinePropertyTable_ && (i == numSuccessors() - 1))
            return fallbackPrepBlock_;
        else
            return dispatchTable_[i].block;
    }

    static MPolyInlineDispatch *New(MDefinition *ins) {
        return new MPolyInlineDispatch(ins);
    }

    static MPolyInlineDispatch *New(MDefinition *ins, InlinePropertyTable *inlinePropTable,
                                    MBasicBlock *fallbackPrepBlock,
                                    MBasicBlock *fallbackMidBlock,
                                    MBasicBlock *fallbackEndBlock)
    {
        return new MPolyInlineDispatch(ins, inlinePropTable,
                                       fallbackPrepBlock,
                                       fallbackMidBlock,
                                       fallbackEndBlock);
    }

    size_t numCallees() const {
        return dispatchTable_.length();
    }

    void addCallee(MConstant *funcConst, MBasicBlock *block) {
        dispatchTable_.append(Entry(funcConst, block));
    }

    MConstant *getFunctionConstant(size_t i) const {
        JS_ASSERT(i < numCallees());
        return dispatchTable_[i].funcConst;
    }

    JSFunction *getFunction(size_t i) const {
        return getFunctionConstant(i)->value().toObject().toFunction();
    }

    MBasicBlock *getFunctionBlock(size_t i) const {
        JS_ASSERT(i < numCallees());
        return dispatchTable_[i].block;
    }

    MBasicBlock *getFunctionBlock(JSFunction *func) const {
        for (size_t i = 0; i < numCallees(); i++) {
            if (getFunction(i) == func)
                return getFunctionBlock(i);
        }
        JS_NOT_REACHED("Bad function lookup!");
    }

    InlinePropertyTable *propTable() const {
        return inlinePropertyTable_;
    }

    MBasicBlock *fallbackPrepBlock() const {
        JS_ASSERT(inlinePropertyTable_ != NULL);
        return fallbackPrepBlock_;
    }

    MBasicBlock *fallbackMidBlock() const {
        JS_ASSERT(inlinePropertyTable_ != NULL);
        return fallbackMidBlock_;
    }

    MBasicBlock *fallbackEndBlock() const {
        JS_ASSERT(inlinePropertyTable_ != NULL);
        return fallbackEndBlock_;
    }

    MDefinition *input() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
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

    static MGetElementCache *New(MDefinition *obj, MDefinition *value, bool monitoredResult) {
        return new MGetElementCache(obj, value, monitoredResult);
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

    static MBindNameCache *New(MDefinition *scopeChain, PropertyName *name, JSScript *script,
                               jsbytecode *pc) {
        return new MBindNameCache(scopeChain, name, script, pc);
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

    static MGuardShape *New(MDefinition *obj, Shape *shape, BailoutKind bailoutKind) {
        return new MGuardShape(obj, shape, bailoutKind);
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
    bool congruentTo(MDefinition * const &ins) const {
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

    static MGuardObjectType *New(MDefinition *obj, types::TypeObject *typeObject,
                                 bool bailOnEquality) {
        return new MGuardObjectType(obj, typeObject, bailOnEquality);
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
    bool congruentTo(MDefinition * const &ins) const {
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

    static MGuardClass *New(MDefinition *obj, const Class *clasp) {
        return new MGuardClass(obj, clasp);
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
    bool congruentTo(MDefinition * const &ins) const {
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

    static MLoadSlot *New(MDefinition *slots, uint32_t slot) {
        return new MLoadSlot(slots, slot);
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

    bool congruentTo(MDefinition * const &ins) const {
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
    }

    INSTRUCTION_HEADER(FunctionEnvironment)

    static MFunctionEnvironment *New(MDefinition *function) {
        return new MFunctionEnvironment(function);
    }

    MDefinition *function() const {
        return getOperand(0);
    }
};

// Loads the current js::ForkJoinSlice*.
// Only applicable in ParallelExecution.
class MParSlice
  : public MNullaryInstruction
{
  public:
    MParSlice()
        : MNullaryInstruction()
    {
        setResultType(MIRType_ForkJoinSlice);
    }

    INSTRUCTION_HEADER(ParSlice);

    AliasSet getAliasSet() const {
        // Indicate that this instruction reads nothing, stores nothing.
        // (For all intents and purposes)
        return AliasSet::None();
    }
};

// Store to vp[slot] (slots that are not inline in an object).
class MStoreSlot
  : public MBinaryInstruction,
    public SingleObjectPolicy
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

    static MStoreSlot *New(MDefinition *slots, uint32_t slot, MDefinition *value) {
        return new MStoreSlot(slots, slot, value, false);
    }
    static MStoreSlot *NewBarriered(MDefinition *slots, uint32_t slot, MDefinition *value) {
        return new MStoreSlot(slots, slot, value, true);
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

    MGetNameCache(MDefinition *obj, HandlePropertyName name, AccessKind kind)
      : MUnaryInstruction(obj),
        name_(name),
        kind_(kind)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(GetNameCache)

    static MGetNameCache *New(MDefinition *obj, HandlePropertyName name, AccessKind kind) {
        return new MGetNameCache(obj, name, kind);
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

    MCallGetIntrinsicValue(HandlePropertyName name)
      : name_(name)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CallGetIntrinsicValue)

    static MCallGetIntrinsicValue *New(HandlePropertyName name) {
        return new MCallGetIntrinsicValue(name);
    }
    PropertyName *name() const {
        return name_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
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

    static MCallsiteCloneCache *New(MDefinition *callee, jsbytecode *callPc) {
        return new MCallsiteCloneCache(callee, callPc);
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
    MSetPropertyInstruction(MDefinition *obj, MDefinition *value, HandlePropertyName name,
                            bool strict)
      : MBinaryInstruction(obj, value),
        name_(name), strict_(strict), needsBarrier_(true)
    {}

  public:
    MDefinition *obj() const {
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
    MDeleteProperty(MDefinition *val, HandlePropertyName name)
      : MUnaryInstruction(val),
        name_(name)
    {
        setResultType(MIRType_Boolean);
    }

  public:
    INSTRUCTION_HEADER(DeleteProperty)

    static MDeleteProperty *New(MDefinition *obj, HandlePropertyName name) {
        return new MDeleteProperty(obj, name);
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

// Note: This uses CallSetElementPolicy to always box its second input,
// ensuring we don't need two LIR instructions to lower this.
class MCallSetProperty
  : public MSetPropertyInstruction,
    public CallSetElementPolicy
{
    MCallSetProperty(MDefinition *obj, MDefinition *value, HandlePropertyName name, bool strict)
      : MSetPropertyInstruction(obj, value, name, strict)
    {
    }

  public:
    INSTRUCTION_HEADER(CallSetProperty)

    static MCallSetProperty *New(MDefinition *obj, MDefinition *value, HandlePropertyName name, bool strict) {
        return new MCallSetProperty(obj, value, name, strict);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MSetPropertyCache
  : public MSetPropertyInstruction,
    public SingleObjectPolicy
{
    MSetPropertyCache(MDefinition *obj, MDefinition *value, HandlePropertyName name, bool strict)
      : MSetPropertyInstruction(obj, value, name, strict)
    {
    }

  public:
    INSTRUCTION_HEADER(SetPropertyCache)

    static MSetPropertyCache *New(MDefinition *obj, MDefinition *value, HandlePropertyName name, bool strict) {
        return new MSetPropertyCache(obj, value, name, strict);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MSetElementCache
  : public MSetElementInstruction,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
    bool strict_;

    MSetElementCache(MDefinition *obj, MDefinition *index, MDefinition *value, bool strict)
      : MSetElementInstruction(obj, index, value),
        strict_(strict)
    {
    }

  public:
    INSTRUCTION_HEADER(SetElementCache);

    static MSetElementCache *New(MDefinition *obj, MDefinition *index, MDefinition *value,
                                 bool strict) {
        return new MSetElementCache(obj, index, value, strict);
    }

    bool strict() const {
        return strict_;
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MCallGetProperty
  : public MUnaryInstruction,
    public BoxInputsPolicy
{
    CompilerRootPropertyName name_;
    bool idempotent_;

    MCallGetProperty(MDefinition *value, HandlePropertyName name)
      : MUnaryInstruction(value), name_(name),
        idempotent_(false)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(CallGetProperty)

    static MCallGetProperty *New(MDefinition *value, HandlePropertyName name) {
        return new MCallGetProperty(value, name);
    }
    MDefinition *value() const {
        return getOperand(0);
    }
    PropertyName *name() const {
        return name_;
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

    static MCallGetElement *New(MDefinition *lhs, MDefinition *rhs) {
        return new MCallGetElement(lhs, rhs);
    }
    TypePolicy *typePolicy() {
        return this;
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

    static MCallSetElement *New(MDefinition *object, MDefinition *index, MDefinition *value) {
        return new MCallSetElement(object, index, value);
    }

    TypePolicy *typePolicy() {
        return this;
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

    static MCallInitElementArray *New(MDefinition *obj, uint32_t index, MDefinition *val)
    {
        return new MCallInitElementArray(obj, index, val);
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
};

class MSetDOMProperty
  : public MAryInstruction<2>,
    public MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >
{
    const JSJitPropertyOp func_;

    MSetDOMProperty(const JSJitPropertyOp func, MDefinition *obj, MDefinition *val)
      : func_(func)
    {
        setOperand(0, obj);
        setOperand(1, val);
    }

  public:
    INSTRUCTION_HEADER(SetDOMProperty)

    static MSetDOMProperty *New(const JSJitPropertyOp func, MDefinition *obj, MDefinition *val)
    {
        return new MSetDOMProperty(func, obj, val);
    }

    const JSJitPropertyOp fun() {
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
};

class MGetDOMProperty
  : public MAryInstruction<2>,
    public ObjectPolicy<0>
{
    const JSJitInfo *info_;

    MGetDOMProperty(const JSJitInfo *jitinfo, MDefinition *obj, MDefinition *guard)
      : info_(jitinfo)
    {
        JS_ASSERT(jitinfo);

        setOperand(0, obj);

        // Pin the guard as an operand if we want to hoist later
        setOperand(1, guard);

        // We are movable iff the jitinfo says we can be.
        if (jitinfo->isPure)
            setMovable();

        setResultType(MIRType_Value);
    }

  protected:
    const JSJitInfo *info() const {
        return info_;
    }

  public:
    INSTRUCTION_HEADER(GetDOMProperty)

    static MGetDOMProperty *New(const JSJitInfo *info, MDefinition *obj, MDefinition *guard)
    {
        return new MGetDOMProperty(info, obj, guard);
    }

    const JSJitPropertyOp fun() {
        return info_->op;
    }
    bool isInfallible() const {
        return info_->isInfallible;
    }
    bool isDomConstant() const {
        return info_->isConstant;
    }
    bool isDomPure() const {
        return info_->isPure;
    }
    MDefinition *object() {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool congruentTo(MDefinition *const &ins) const {
        if (!isDomPure())
            return false;

        if (!ins->isGetDOMProperty())
            return false;

        // Checking the jitinfo is the same as checking the constant function
        if (!(info() == ins->toGetDOMProperty()->info()))
            return false;

        return congruentIfOperandsEqual(ins);
    }

    AliasSet getAliasSet() const {
        // The whole point of constancy is that it's non-effectful and doesn't
        // conflict with anything
        if (isDomConstant())
            return AliasSet::None();
        // Pure DOM attributes can only alias things that alias the world or
        // explicitly alias DOM properties.
        if (isDomPure())
            return AliasSet::Load(AliasSet::DOMProperty);
        return AliasSet::Store(AliasSet::Any);
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

    static MStringLength *New(MDefinition *string) {
        return new MStringLength(string);
    }

    MDefinition *foldsTo(bool useValueNumbers);

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *string() const {
        return getOperand(0);
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // The string |length| property is immutable, so there is no
        // implicit dependency.
        return AliasSet::None();
    }
};

// Inlined version of Math.floor().
class MFloor
  : public MUnaryInstruction,
    public DoublePolicy<0>
{
  public:
    MFloor(MDefinition *num)
      : MUnaryInstruction(num)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

    INSTRUCTION_HEADER(Floor)

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

// Inlined version of Math.round().
class MRound
  : public MUnaryInstruction,
    public DoublePolicy<0>
{
  public:
    MRound(MDefinition *num)
      : MUnaryInstruction(num)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

    INSTRUCTION_HEADER(Round)

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

    static MIteratorStart *New(MDefinition *obj, uint8_t flags) {
        return new MIteratorStart(obj, flags);
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

    static MIteratorNext *New(MDefinition *iter) {
        return new MIteratorNext(iter);
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

    static MIteratorMore *New(MDefinition *iter) {
        return new MIteratorMore(iter);
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

    static MIteratorEnd *New(MDefinition *iter) {
        return new MIteratorEnd(iter);
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
  public:
    MIn(MDefinition *key, MDefinition *obj)
      : MBinaryInstruction(key, obj)
    {
        setResultType(MIRType_Boolean);
    }

    INSTRUCTION_HEADER(In)

    TypePolicy *typePolicy() {
        return this;
    }
};


// Test whether the index is in the array bounds or a hole.
class MInArray
  : public MQuaternaryInstruction,
    public ObjectPolicy<3>
{
    bool needsHoleCheck_;

    MInArray(MDefinition *elements, MDefinition *index,
             MDefinition *initLength, MDefinition *object,
             bool needsHoleCheck)
      : MQuaternaryInstruction(elements, index, initLength, object),
        needsHoleCheck_(needsHoleCheck)
    {
        setResultType(MIRType_Boolean);
        setMovable();
        JS_ASSERT(elements->type() == MIRType_Elements);
        JS_ASSERT(index->type() == MIRType_Int32);
        JS_ASSERT(initLength->type() == MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(InArray)

    static MInArray *New(MDefinition *elements, MDefinition *index,
                         MDefinition *initLength, MDefinition *object,
                         bool needsHoleCheck)
    {
        return new MInArray(elements, index, initLength, object, needsHoleCheck);
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
    bool needsNegativeIntCheck() const;
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

  public:
    MInstanceOf(MDefinition *obj, JSObject *proto)
      : MUnaryInstruction(obj),
        protoObj_(proto)
    {
        setResultType(MIRType_Boolean);
    }

    INSTRUCTION_HEADER(InstanceOf)

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
  public:
    MCallInstanceOf(MDefinition *obj, MDefinition *proto)
      : MBinaryInstruction(obj, proto)
    {
        setResultType(MIRType_Boolean);
    }

    INSTRUCTION_HEADER(CallInstanceOf)

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

    static MArgumentsLength *New() {
        return new MArgumentsLength();
    }

    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        // Arguments |length| cannot be mutated by Ion Code.
        return AliasSet::None();
   }
};

// This MIR instruction is used to get an argument from the actual arguments.
class MGetArgument
  : public MUnaryInstruction,
    public IntPolicy<0>
{
    MGetArgument(MDefinition *idx)
      : MUnaryInstruction(idx)
    {
        setResultType(MIRType_Value);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(GetArgument)

    static MGetArgument *New(MDefinition *idx) {
        return new MGetArgument(idx);
    }

    MDefinition *index() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }
    bool congruentTo(MDefinition *const &ins) const {
        return congruentIfOperandsEqual(ins);
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
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
    MRest(MDefinition *numActuals, unsigned numFormals, JSObject *templateObject)
      : MUnaryInstruction(numActuals),
        MRestCommon(numFormals, templateObject)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(templateObject));
    }

  public:
    INSTRUCTION_HEADER(Rest);

    static MRest *New(MDefinition *numActuals, unsigned numFormals, JSObject *templateObject) {
        return new MRest(numActuals, numFormals, templateObject);
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
};

class MParRest
  : public MBinaryInstruction,
    public MRestCommon,
    public IntPolicy<1>
{
    MParRest(MDefinition *parSlice, MDefinition *numActuals, unsigned numFormals,
             JSObject *templateObject)
      : MBinaryInstruction(parSlice, numActuals),
        MRestCommon(numFormals, templateObject)
    {
        setResultType(MIRType_Object);
        setResultTypeSet(MakeSingletonTypeSet(templateObject));
    }

  public:
    INSTRUCTION_HEADER(ParRest);

    static MParRest *New(MDefinition *parSlice, MDefinition *numActuals, unsigned numFormals,
                         JSObject *templateObject) {
        return new MParRest(parSlice, numActuals, numFormals, templateObject);
    }
    static MParRest *New(MDefinition *parSlice, MRest *rest) {
        return new MParRest(parSlice, rest->numActuals(), rest->numFormals(), rest->templateObject());
    }

    MDefinition *parSlice() const {
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
};

class MParWriteGuard
  : public MBinaryInstruction,
    public ObjectPolicy<1>
{
    MParWriteGuard(MDefinition *parThreadContext,
                   MDefinition *obj)
      : MBinaryInstruction(parThreadContext, obj)
    {
        setResultType(MIRType_None);
        setGuard();
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(ParWriteGuard);

    static MParWriteGuard *New(MDefinition *parThreadContext, MDefinition *obj) {
        return new MParWriteGuard(parThreadContext, obj);
    }
    MDefinition *parSlice() const {
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
};

class MParDump
  : public MUnaryInstruction,
    public BoxPolicy<0>
{
  public:
    INSTRUCTION_HEADER(ParDump);

    MParDump(MDefinition *v)
      : MUnaryInstruction(v)
    {
        setResultType(MIRType_None);
    }

    MDefinition *value() const {
        return getOperand(0);
    }

    TypePolicy *typePolicy() {
        return this;
    }
};


// Given a value, guard that the value is in a particular TypeSet, then returns
// that value.
class MTypeBarrier
  : public MUnaryInstruction,
    public BoxInputsPolicy
{
    BailoutKind bailoutKind_;

    MTypeBarrier(MDefinition *def, types::StackTypeSet *types, BailoutKind bailoutKind)
      : MUnaryInstruction(def)
    {
        JS_ASSERT(!types->unknown());
        setResultType(MIRType_Value);
        setResultTypeSet(types);
        setGuard();
        setMovable();
        bailoutKind_ = bailoutKind;
    }

  public:
    INSTRUCTION_HEADER(TypeBarrier)

    static MTypeBarrier *New(MDefinition *def, types::StackTypeSet *types) {
        BailoutKind bailoutKind = def->isEffectful()
                                  ? Bailout_TypeBarrier
                                  : Bailout_Normal;
        return new MTypeBarrier(def, types, bailoutKind);
    }
    static MTypeBarrier *New(MDefinition *def, types::StackTypeSet *types,
                             BailoutKind bailoutKind) {
        return new MTypeBarrier(def, types, bailoutKind);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    bool congruentTo(MDefinition * const &def) const {
        return false;
    }
    MDefinition *input() const {
        return getOperand(0);
    }
    BailoutKind bailoutKind() const {
        return bailoutKind_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
    virtual bool neverHoist() const {
        return resultTypeSet()->empty();
    }
};

// Like MTypeBarrier, guard that the value is in the given type set. This is
// used before property writes to ensure the value being written is represented
// in the property types for the object.
class MMonitorTypes : public MUnaryInstruction, public BoxInputsPolicy
{
    const types::StackTypeSet *typeSet_;

    MMonitorTypes(MDefinition *def, const types::StackTypeSet *types)
      : MUnaryInstruction(def),
        typeSet_(types)
    {
        setGuard();
        JS_ASSERT(!types->unknown());
    }

  public:
    INSTRUCTION_HEADER(MonitorTypes)

    static MMonitorTypes *New(MDefinition *def, const types::StackTypeSet *types) {
        return new MMonitorTypes(def, types);
    }

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *input() const {
        return getOperand(0);
    }
    const types::StackTypeSet *typeSet() const {
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
    MPostWriteBarrier(MDefinition *obj, MDefinition *value)
      : MBinaryInstruction(obj, value)
    {
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(PostWriteBarrier)

    static MPostWriteBarrier *New(MDefinition *obj, MDefinition *value) {
        return new MPostWriteBarrier(obj, value);
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

    static MNewSlots *New(unsigned nslots) {
        return new MNewSlots(nslots);
    }
    unsigned nslots() const {
        return nslots_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MNewDeclEnvObject : public MNullaryInstruction
{
    CompilerRootObject templateObj_;

    MNewDeclEnvObject(HandleObject templateObj)
      : MNullaryInstruction(),
        templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewDeclEnvObject);

    static MNewDeclEnvObject *New(HandleObject templateObj) {
        return new MNewDeclEnvObject(templateObj);
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

    MNewCallObject(HandleObject templateObj, MDefinition *slots)
      : MUnaryInstruction(slots),
        templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewCallObject)

    static MNewCallObject *New(HandleObject templateObj, MDefinition *slots) {
        return new MNewCallObject(templateObj, slots);
    }

    MDefinition *slots() {
        return getOperand(0);
    }
    JSObject *templateObject() {
        return templateObj_;
    }
    AliasSet getAliasSet() const {
        return AliasSet::None();
    }
};

class MParNewCallObject : public MBinaryInstruction
{
    CompilerRootObject templateObj_;

    MParNewCallObject(MDefinition *parSlice,
                      JSObject *templateObj, MDefinition *slots)
        : MBinaryInstruction(parSlice, slots),
          templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(ParNewCallObject);

    static MParNewCallObject *New(MDefinition *parSlice,
                                  JSObject *templateObj,
                                  MDefinition *slots) {
        return new MParNewCallObject(parSlice, templateObj, slots);
    }

    static MParNewCallObject *New(MDefinition *parSlice,
                                  MNewCallObject *originalInstruction) {
        return New(parSlice,
                   originalInstruction->templateObject(),
                   originalInstruction->slots());
    }

    MDefinition *parSlice() const {
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

    MNewStringObject(MDefinition *input, HandleObject templateObj)
      : MUnaryInstruction(input),
        templateObj_(templateObj)
    {
        setResultType(MIRType_Object);
    }

  public:
    INSTRUCTION_HEADER(NewStringObject)

    static MNewStringObject *New(MDefinition *input, HandleObject templateObj) {
        return new MNewStringObject(input, templateObj);
    }

    MDefinition *input() const {
        return getOperand(0);
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
        JS_ASSERT_IF(type != Inline_Exit, script != NULL);
        JS_ASSERT_IF(type == Inline_Enter, inlineLevel != 0);
        setGuard();
    }

  public:
    INSTRUCTION_HEADER(FunctionBoundary)

    static MFunctionBoundary *New(JSScript *script, Type type,
                                  unsigned inlineLevel = 0) {
        return new MFunctionBoundary(script, type, inlineLevel);
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
    static MEnclosingScope *New(MDefinition *obj) {
        return new MEnclosingScope(obj);
    }

    AliasSet getAliasSet() const {
        // ScopeObject reserved slots are immutable.
        return AliasSet::None();
    }
};

// Creates a dense array of the given length.
//
// Note: the template object should be an *empty* dense array!
class MParNewDenseArray : public MBinaryInstruction
{
    CompilerRootObject templateObject_;

  public:
    INSTRUCTION_HEADER(ParNewDenseArray);

    MParNewDenseArray(MDefinition *parSlice,
                      MDefinition *length,
                      JSObject *templateObject)
      : MBinaryInstruction(parSlice, length),
        templateObject_(templateObject)
    {
        setResultType(MIRType_Object);
    }

    MDefinition *parSlice() const {
        return getOperand(0);
    }

    MDefinition *length() const {
        return getOperand(1);
    }

    JSObject *templateObject() const {
        return templateObject_;
    }
};

// A resume point contains the information needed to reconstruct the interpreter
// state from a position in the JIT. See the big comment near resumeAfter() in
// IonBuilder.cpp.
class MResumePoint : public MNode, public InlineForwardListNode<MResumePoint>
{
  public:
    enum Mode {
        ResumeAt,    // Resume until before the current instruction
        ResumeAfter, // Resume after the current instruction
        Outer        // State before inlining.
    };

  private:
    friend class MBasicBlock;

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
    bool init() {
        return operands_.init(stackDepth_);
    }

    // Overwrites an operand without updating its Uses.
    void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index < stackDepth_);
        operands_[index].set(operand, this, index);
        operand->addUse(&operands_[index]);
    }

    void clearOperand(size_t index) {
        JS_ASSERT(index < stackDepth_);
        operands_[index].set(NULL, this, index);
    }

    MUse *getUseFor(size_t index) {
        return &operands_[index];
    }

  public:
    static MResumePoint *New(MBasicBlock *block, jsbytecode *pc, MResumePoint *parent, Mode mode);

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

    static MIsCallable *New(MDefinition *obj) {
        return new MIsCallable(obj);
    }

    MDefinition *object() const {
        return getOperand(0);
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
    static MAsmJSNeg *NewAsmJS(MDefinition *op, MIRType type) {
        return new MAsmJSNeg(op, type);
    }

    MDefinition *input() const {
        return getOperand(0);
    }
};

class MAsmJSUDiv : public MBinaryInstruction
{
    MAsmJSUDiv(MDefinition *left, MDefinition *right)
      : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(AsmJSUDiv);
    static MAsmJSUDiv *New(MDefinition *left, MDefinition *right) {
        return new MAsmJSUDiv(left, right);
    }
};

class MAsmJSUMod : public MBinaryInstruction
{
    MAsmJSUMod(MDefinition *left, MDefinition *right)
       : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Int32);
        setMovable();
    }

  public:
    INSTRUCTION_HEADER(AsmJSUMod);
    static MAsmJSUMod *New(MDefinition *left, MDefinition *right) {
        return new MAsmJSUMod(left, right);
    }
};

class MAsmJSLoadHeap : public MUnaryInstruction
{
    MAsmJSLoadHeap(ArrayBufferView::ViewType vt, MDefinition *ptr)
      : MUnaryInstruction(ptr), viewType_(vt)
    {
        if (vt == ArrayBufferView::TYPE_FLOAT32 || vt == ArrayBufferView::TYPE_FLOAT64)
            setResultType(MIRType_Double);
        else
            setResultType(MIRType_Int32);
    }

    ArrayBufferView::ViewType viewType_;

  public:
    INSTRUCTION_HEADER(AsmJSLoadHeap);

    static MAsmJSLoadHeap *New(ArrayBufferView::ViewType vt, MDefinition *ptr) {
        return new MAsmJSLoadHeap(vt, ptr);
    }

    ArrayBufferView::ViewType viewType() const { return viewType_; }
    MDefinition *ptr() const { return getOperand(0); }
};

class MAsmJSStoreHeap : public MBinaryInstruction
{
    MAsmJSStoreHeap(ArrayBufferView::ViewType vt, MDefinition *ptr, MDefinition *v)
      : MBinaryInstruction(ptr, v), viewType_(vt)
    {}

    ArrayBufferView::ViewType viewType_;

  public:
    INSTRUCTION_HEADER(AsmJSStoreHeap);

    static MAsmJSStoreHeap *New(ArrayBufferView::ViewType vt, MDefinition *ptr, MDefinition *v) {
        return new MAsmJSStoreHeap(vt, ptr, v);
    }

    ArrayBufferView::ViewType viewType() const { return viewType_; }
    MDefinition *ptr() const { return getOperand(0); }
    MDefinition *value() const { return getOperand(1); }
};

class MAsmJSLoadGlobalVar : public MNullaryInstruction
{
    MAsmJSLoadGlobalVar(MIRType type, unsigned globalDataOffset)
      : globalDataOffset_(globalDataOffset)
    {
        JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double);
        setResultType(type);
    }

    unsigned globalDataOffset_;

  public:
    INSTRUCTION_HEADER(AsmJSLoadGlobalVar);

    static MAsmJSLoadGlobalVar *New(MIRType type, unsigned globalDataOffset) {
        return new MAsmJSLoadGlobalVar(type, globalDataOffset);
    }

    unsigned globalDataOffset() const { return globalDataOffset_; }
};

class MAsmJSStoreGlobalVar : public MUnaryInstruction
{
    MAsmJSStoreGlobalVar(unsigned globalDataOffset, MDefinition *v)
      : MUnaryInstruction(v), globalDataOffset_(globalDataOffset)
    {}

    unsigned globalDataOffset_;

  public:
    INSTRUCTION_HEADER(AsmJSStoreGlobalVar);

    static MAsmJSStoreGlobalVar *New(unsigned globalDataOffset, MDefinition *v) {
        return new MAsmJSStoreGlobalVar(globalDataOffset, v);
    }

    unsigned globalDataOffset() const { return globalDataOffset_; }
    MDefinition *value() const { return getOperand(0); }
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

    static MAsmJSLoadFuncPtr *New(unsigned globalDataOffset, MDefinition *index) {
        return new MAsmJSLoadFuncPtr(globalDataOffset, index);
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

    static MAsmJSLoadFFIFunc *New(unsigned globalDataOffset) {
        return new MAsmJSLoadFFIFunc(globalDataOffset);
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

    static MAsmJSParameter *New(ABIArg abi, MIRType mirType) {
        return new MAsmJSParameter(abi, mirType);
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
    static MAsmJSReturn *New(MDefinition *ins) {
        return new MAsmJSReturn(ins);
    }
};

class MAsmJSVoidReturn : public MAryControlInstruction<0, 0>
{
  public:
    INSTRUCTION_HEADER(AsmJSVoidReturn);
    static MAsmJSVoidReturn *New() {
        return new MAsmJSVoidReturn();
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
    static MAsmJSPassStackArg *New(uint32_t spOffset, MDefinition *ins) {
        return new MAsmJSPassStackArg(spOffset, ins);
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

class MAsmJSCall : public MInstruction
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
            const void *builtin_;
        } u;
      public:
        Callee() {}
        Callee(Label *callee) : which_(Internal) { u.internal_ = callee; }
        Callee(MDefinition *callee) : which_(Dynamic) { u.dynamic_ = callee; }
        Callee(const void *callee) : which_(Builtin) { u.builtin_ = callee; }
        Which which() const { return which_; }
        Label *internal() const { JS_ASSERT(which_ == Internal); return u.internal_; }
        MDefinition *dynamic() const { JS_ASSERT(which_ == Dynamic); return u.dynamic_; }
        const void *builtin() const { JS_ASSERT(which_ == Builtin); return u.builtin_; }
    };

  private:
    struct Operand {
        AnyRegister reg;
        MUse use;
    };

    Callee callee_;
    size_t numOperands_;
    MUse *operands_;
    size_t numArgs_;
    AnyRegister *argRegs_;
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

    static MAsmJSCall *New(Callee callee, const Args &args, MIRType resultType, size_t spIncrement);

    size_t numOperands() const {
        return numOperands_;
    }
    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index < numOperands_);
        return operands_[index].producer();
    }
    size_t numArgs() const {
        return numArgs_;
    }
    AnyRegister registerForArg(size_t index) const {
        JS_ASSERT(index < numArgs_);
        return argRegs_[index];
    }
    Callee callee() const {
        return callee_;
    }
    size_t dynamicCalleeOperandIndex() const {
        JS_ASSERT(callee_.which() == Callee::Dynamic);
        JS_ASSERT(numArgs_ == numOperands_ - 1);
        return numArgs_;
    }
    size_t spIncrement() const {
        return spIncrement_;
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
    static MAsmJSCheckOverRecursed *New(Label *onError) { return new MAsmJSCheckOverRecursed(onError); }
    Label *onError() const { return onError_; }
};

#undef INSTRUCTION_HEADER

// Implement opcode casts now that the compiler can see the inheritance.
#define OPCODE_CASTS(opcode)                                                \
    M##opcode *MDefinition::to##opcode()                                    \
    {                                                                       \
        JS_ASSERT(is##opcode());                                            \
        return static_cast<M##opcode *>(this);                              \
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
bool ElementAccessIsTypedArray(MDefinition *obj, MDefinition *id, int *arrayType);
bool ElementAccessIsPacked(JSContext *cx, MDefinition *obj);
bool ElementAccessHasExtraIndexedProperty(JSContext *cx, MDefinition *obj);
MIRType DenseNativeElementType(JSContext *cx, MDefinition *obj);
bool PropertyReadNeedsTypeBarrier(JSContext *cx, types::TypeObject *object, PropertyName *name,
                                  types::StackTypeSet *observed, bool updateObserved = true);
bool PropertyReadNeedsTypeBarrier(JSContext *cx, MDefinition *obj, PropertyName *name,
                                  types::StackTypeSet *observed);
bool PropertyReadIsIdempotent(JSContext *cx, MDefinition *obj, PropertyName *name);
void AddObjectsForPropertyRead(JSContext *cx, MDefinition *obj, PropertyName *name,
                               types::StackTypeSet *observed);
bool PropertyWriteNeedsTypeBarrier(JSContext *cx, MBasicBlock *current, MDefinition **pobj,
                                   PropertyName *name, MDefinition **pvalue,
                                   bool canModify = true);

} // namespace ion
} // namespace js

#endif // jsion_mir_h__

