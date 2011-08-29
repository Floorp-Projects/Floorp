/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsion_mir_h__
#define jsion_mir_h__

// This file declares everything needed to build actual MIR instructions: the
// actual opcodes and instructions themselves, the instruction interface, and
// use chains.

#include "jscntxt.h"
#include "TypeOracle.h"
#include "TypePolicy.h"
#include "IonAllocPolicy.h"
#include "InlineList.h"
#include "MOpcodes.h"
#include "FixedArityList.h"
#include "IonMacroAssembler.h"

namespace js {
namespace ion {

static const inline
MIRType MIRTypeFromValue(const js::Value &vp)
{
    if (vp.isDouble())
        return MIRType_Double;
    switch (vp.extractNonDoubleType()) {
      case JSVAL_TYPE_INT32:
        return MIRType_Int32;
      case JSVAL_TYPE_UNDEFINED:
        return MIRType_Undefined;
      case JSVAL_TYPE_STRING:
        return MIRType_String;
      case JSVAL_TYPE_BOOLEAN:
        return MIRType_Boolean;
      case JSVAL_TYPE_NULL:
        return MIRType_Null;
      case JSVAL_TYPE_OBJECT:
        return MIRType_Object;
      default:
        JS_NOT_REACHED("unexpected jsval type");
        return MIRType_None;
    }
}

#define MIR_FLAG_LIST(_)                                                        \
    _(InWorklist)                                                               \
    _(EmittedAtUses)                                                            \
    _(LoopInvariant)                                                            \
    _(Commutative)                                                              \
    _(Idempotent)    /* The instruction has no side-effects. */                 \
    _(NeverHoisted)  /* Don't hoist, even if loop invariant */                  \
    _(Lowered)       /* (Debug only) has a virtual register */

class MDefinition;
class MInstruction;
class MBasicBlock;
class MNode;
class MUse;
class MIRGraph;
class MSnapshot;

// Represents a use of a node.
class MUse : public TempObject, public InlineForwardListNode<MUse>
{
    friend class MDefinition;

    MNode *node_;           // The node that is using this operand.
    uint32 index_;          // The index of this operand in its owner.

    MUse(MNode *owner, uint32 index)
      : node_(owner),
        index_(index)
    { }

  public:
    static inline MUse *New(MNode *owner, uint32 index) {
        return new MUse(owner, index);
    }

    MNode *node() const {
        return node_;
    }
    uint32 index() const {
        return index_;
    }
};

typedef InlineForwardList<MUse>::iterator MUseIterator;

// A node is an entry in the MIR graph. It has two kinds:
//   MInstruction: an instruction which appears in the IR stream.
//   MSnapshot: a list of instructions that correspond to the state of the
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
        Snapshot
    };

    MNode() : block_(NULL)
    { }
    MNode(MBasicBlock *block) : block_(block)
    { }

    virtual Kind kind() const = 0;

    // Returns the definition at a given operand.
    virtual MDefinition *getOperand(size_t index) const = 0;
    virtual size_t numOperands() const = 0;

    bool isDefinition() const {
        return kind() == Definition;
    }
    bool isSnapshot() const {
        return kind() == Snapshot;
    }
    MBasicBlock *block() const {
        return block_;
    }

    // Instructions needing to hook into type analysis should return a
    // TypePolicy.
    virtual TypePolicy *typePolicy() {
        return NULL;
    }

    // Replaces an operand, taking care to update use chains. No memory is
    // allocated; the existing data structures are re-linked.
    MUseIterator replaceOperand(MUseIterator use, MDefinition *ins);
    void replaceOperand(size_t index, MDefinition *ins);

    inline MDefinition *toDefinition();
    inline MSnapshot *toSnapshot();

  protected:
    // Sets a raw operand, ignoring updating use information.
    virtual void setOperand(size_t index, MDefinition *operand) = 0;

    // Initializes an operand for the first time.
    inline void initOperand(size_t index, MDefinition *ins);
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
    InlineForwardList<MUse> uses_; // Use chain.
    uint32 id_;                    // Instruction ID, which after block re-ordering
                                   // is sorted within a basic block.
    uint32 valueNumber_;           // The instruction's value number (see GVN for details in use)
    MIRType resultType_;           // Actual result type.
    uint32 usedTypes_;             // Set of used types.
    uint32 flags_;                 // Bit flags.

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

    bool hasFlags(uint32 flags) const {
        return (flags_ & flags) == flags;
    }
    void removeFlags(uint32 flags) {
        flags_ &= ~flags;
    }
    void setFlags(uint32 flags) {
        flags_ |= flags;
    }

  public:
    MDefinition()
      : id_(0),
        valueNumber_(0),
        resultType_(MIRType_None),
        usedTypes_(0),
        flags_(0)
    { }

    virtual Opcode op() const = 0;
    void printName(FILE *fp);
    virtual void printOpcode(FILE *fp);

    virtual HashNumber valueHash() const;
    virtual bool congruentTo(MDefinition* const &ins) const;
    virtual MDefinition *foldsTo(bool useValueNumbers);

    MNode::Kind kind() const {
        return MNode::Definition;
    }

    uint32 id() const {
        JS_ASSERT(block_);
        return id_;
    }
    void setId(uint32 id) {
        id_ = id;
    }

    uint32 valueNumber() const {
        JS_ASSERT(block_);
        return valueNumber_;
    }

    void setValueNumber(uint32 vn) {
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

    // Returns a normalized type this instruction should be used as. If exactly
    // one type was requested, then that type is returned. If more than one
    // type was requested, then Value is returned.
    MIRType usedAsType() const;

    // Returns the beginning of this definition's use chain.
    MUseIterator usesBegin() const {
        return uses_.begin();
    }

    // Returns the end of this definition's use chain.
    MUseIterator usesEnd() const {
        return uses_.end();
    }

    // Removes a use at the given position
    MUseIterator removeUse(MUseIterator use);

    // Number of uses of this instruction.
    size_t useCount() const;

    bool hasUses() const {
        return !uses_.empty();
    }

    virtual bool isControlInstruction() {
        return false;
    }

    void addUse(MNode *node, size_t index) {
        uses_.pushFront(MUse::New(node, index));
    }
    void replaceAllUsesWith(MDefinition *dom);

    // Adds a use from a node that is being recycled during operand
    // replacement.
    void linkUse(MUse *use) {
        JS_ASSERT(use->node()->getOperand(use->index()) == this);
        uses_.pushFront(use);
    }

  public:   // Functions for analysis phases.
    // Analyzes inputs and uses and updates type information. If type
    // information changed, returns true, otherwise, returns false.
    void addUsedTypes(uint32 types) {
        usedTypes_ = types | usedTypes();
    }
    void useAsType(MIRType type) {
        JS_ASSERT(type < MIRType_Value);
        addUsedTypes(1 << uint32(type));
    }
    uint32 usedTypes() const {
        return usedTypes_;
    }

    void setVirtualRegister(uint32 vreg) {
        id_ = vreg;
#ifdef DEBUG
        setLoweredUnchecked();
#endif
    }
    uint32 virtualRegister() const {
        JS_ASSERT(isLowered());
        return id_;
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
            if (i->node()->isDefinition())
                return i;
        }
        return def_->usesEnd();
    }

  public:
    MUseDefIterator(MDefinition *def)
      : def_(def),
        current_(search(def->usesBegin()))
    {
    }

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
        return current_->node()->toDefinition();
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
    MSnapshot *snapshot_;

  public:
    MInstruction() : snapshot_(NULL)
    { }

    virtual bool accept(MInstructionVisitor *visitor) = 0;

    void setSnapshot(MSnapshot *snapshot) {
        JS_ASSERT(!snapshot_);
        snapshot_ = snapshot;
    }
    MSnapshot *snapshot() const {
        return snapshot_;
    }
};

#define INSTRUCTION_HEADER(opcode)                                          \
    Opcode op() const {                                                     \
        return MDefinition::Op_##opcode;                                    \
    }                                                                       \
    bool accept(MInstructionVisitor *visitor) {                             \
        return visitor->visit##opcode(this);                                \
    }

template <size_t Arity>
class MAryInstruction : public MInstruction
{
  protected:
    FixedArityList<MDefinition*, Arity> operands_;

    void setOperand(size_t index, MDefinition *operand) {
        operands_[index] = operand;
    }

  public:
    MDefinition *getOperand(size_t index) const {
        return operands_[index];
    }
    size_t numOperands() const {
        return Arity;
    }
};

// Marks the start of where fallible instructions can go.
class MStart : public MAryInstruction<0>
{
  public:
    INSTRUCTION_HEADER(Start);
    static MStart *New() {
        return new MStart;
    }
};

// A constant js::Value.
class MConstant : public MAryInstruction<0>
{
    js::Value value_;
    uint32 constantPoolIndex_;

    MConstant(const Value &v);

  public:
    INSTRUCTION_HEADER(Constant);
    static MConstant *New(const Value &v);

    const js::Value &value() const {
        return value_;
    }
    const js::Value *vp() const {
        return &value_;
    }
    void setConstantPoolIndex(uint32 index) {
        constantPoolIndex_ = index;
    }
    uint32 constantPoolIndex() const {
        JS_ASSERT(hasConstantPoolIndex());
        return constantPoolIndex_;
    }
    bool hasConstantPoolIndex() const {
        return !!constantPoolIndex_;
    }

    void printOpcode(FILE *fp);

    HashNumber valueHash() const;
    bool congruentTo(MDefinition * const &ins) const;
};

// A reference to a formal parameter.
class MParameter : public MAryInstruction<0>
{
    int32 index_;

  public:
    static const int32 THIS_SLOT = -1;

    MParameter(int32 index)
      : index_(index)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Parameter);
    static MParameter *New(int32 index);

    int32 index() const {
        return index_;
    }
    void printOpcode(FILE *fp);

    HashNumber valueHash() const;
    bool congruentTo(MDefinition * const &ins) const;
};

class MControlInstruction : public MInstruction
{
  public:
    MControlInstruction()
    { }

    virtual size_t numSuccessors() const = 0;
    virtual MBasicBlock *getSuccessor(size_t i) const = 0;
    virtual void replaceSuccessor(size_t i, MBasicBlock *successor) = 0;
};

class MTableSwitch
  : public MControlInstruction,
    public TableSwitchPolicy
{
    // Contains the cases and default case in order of appearence in the actual switch.
    Vector<MBasicBlock*, 0, IonAllocPolicy> successors_;

    // Contains the consecutive targets of each case.
    // If the code doesn't contain that number, it goes to the default case.
    Vector<MBasicBlock*, 0, IonAllocPolicy> cases_;

    MDefinition *operand_;
    int32 low_;
    int32 high_;
    uint32 defaultCase_;

    MTableSwitch(MDefinition *ins, int32 low, int32 high)
      : successors_(),
        cases_(),
        low_(low),
        high_(high),
        defaultCase_(-1)
    {
        initOperand(0, ins);
    }

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index == 0);
        operand_ = operand;
    }

  public:
    INSTRUCTION_HEADER(TableSwitch);
    static MTableSwitch *New(MDefinition *ins, 
                             int32 low, int32 high);

    size_t numSuccessors() const {
        return successors_.length();
    }
 
    MBasicBlock *getSuccessor(size_t i) const {
        JS_ASSERT(i < numSuccessors());
        return successors_[i];
    }
 
    int32 low() const {
        return low_;
    }

    int32 high() const {
        return high_;
    }

    void replaceSuccessor(size_t i, MBasicBlock *successor) {
        JS_ASSERT(i < numSuccessors());
        successors_[i] = successor;
    }

    MBasicBlock *getDefault() const {
        JS_ASSERT(defaultCase_ != (uint32)-1);
        return getSuccessor(defaultCase_);
    }

    MBasicBlock *getCase(uint32 i) const {
        JS_ASSERT(i < cases_.length());
        return cases_[i];
    }

    size_t numCases() const {
        return high() - low() + 1;
    }

    void addDefault(MBasicBlock *block) {
        JS_ASSERT(defaultCase_ == (uint32)-1);
        defaultCase_ = successors_.length();
        successors_.append(block);
    }

    void addCase(MBasicBlock *block, bool isDefault = false) {
        JS_ASSERT(cases_.length() < (size_t)(high_ - low_ + 1));
        cases_.append(block);
        if (!isDefault)
            successors_.append(block);
    }

    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index == 0);
        return operand_;
    }

    size_t numOperands() const {
        return 1;
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

template <size_t Arity>
class MAryControlInstruction : public MControlInstruction
{
    FixedArityList<MDefinition *, Arity> operands_;

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        operands_[index] = operand;
    }

  public:
    MDefinition *getOperand(size_t index) const {
        return operands_[index];
    }
    size_t numOperands() const {
        return Arity;
    }
};

// Jump to the start of another basic block.
class MGoto : public MAryControlInstruction<0>
{
    FixedArityList<MBasicBlock *, 1> successors_;

    MGoto(MBasicBlock *target)
    {
        successors_[0] = target;
    }

  public:
    INSTRUCTION_HEADER(Goto);
    static MGoto *New(MBasicBlock *target);

    size_t numSuccessors() const {
        return 1;
    }

    MBasicBlock *getSuccessor(size_t i) const {
        return successors_[i];
    }

    void replaceSuccessor(size_t i, MBasicBlock *succ) {
        successors_[i] = succ;
    }

    MBasicBlock *target() {
        return successors_[0];
    }
};

// Tests if the input instruction evaluates to true or false, and jumps to the
// start of a corresponding basic block.
class MTest : public MAryControlInstruction<1>
{
    FixedArityList<MBasicBlock *, 2> successors_;

    MTest(MDefinition *ins, MBasicBlock *if_true, MBasicBlock *if_false)
    {
        successors_[0] = if_true;
        successors_[1] = if_false;
        initOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(Test);
    static MTest *New(MDefinition *ins,
                      MBasicBlock *ifTrue, MBasicBlock *ifFalse);

    size_t numSuccessors() const {
        return 2;
    }

    MBasicBlock *getSuccessor(size_t i) const {
        return successors_[i];
    }

    void replaceSuccessor(size_t i, MBasicBlock *successor) {
        successors_[i] = successor;
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }

    bool isControlInstruction() {
        return true;
    }
};

// Returns from this function to the previous caller.
class MReturn
  : public MAryControlInstruction<1>,
    public BoxInputsPolicy
{
    FixedArityList<MBasicBlock *, 0> successors_;

    MReturn(MDefinition *ins)
    {
        initOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(Return);
    static MReturn *New(MDefinition *ins);

    size_t numSuccessors() const {
        return 0;
    }

    MBasicBlock *getSuccessor(size_t i) const {
        return successors_[i];
    }

    void replaceSuccessor(size_t i, MBasicBlock *successor) {
        JS_NOT_REACHED("There are no successors");
    }

    TypePolicy *typePolicy() {
        return this;
    }
};

class MUnaryInstruction : public MAryInstruction<1>
{
  protected:
    MUnaryInstruction(MDefinition *ins)
    {
        initOperand(0, ins);
    }
};

class MBinaryInstruction : public MAryInstruction<2>
{
  protected:
    MBinaryInstruction(MDefinition *left, MDefinition *right)
    {
        initOperand(0, left);
        initOperand(1, right);
    }

    HashNumber valueHash() const
    {
        MDefinition *lhs = getOperand(0);
        MDefinition *rhs = getOperand(1);

        return op() ^ lhs->valueNumber() ^ rhs->valueNumber();
    }

    bool congruentTo(MDefinition *const &ins) const
    {
        if (op() != ins->op())
            return false;

        if (type() != ins->type())
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

class MCompare
  : public MBinaryInstruction,
    public ComparePolicy
{
    AssemblerX86Shared::Condition cond_;

    MCompare(MDefinition *left, MDefinition *right, JSOp op)
      : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Boolean);
        setCondition(op);
    }

  public:
    INSTRUCTION_HEADER(Compare);
    static MCompare *New(MDefinition *left, MDefinition *right, JSOp op);

    void infer(const TypeOracle::Binary &b);
    MIRType specialization() const {
        return specialization_;
    }

    AssemblerX86Shared::Condition condition() {
        return cond_;
    }

    TypePolicy *typePolicy() {
        return this;
    }

  private:
    void setCondition(JSOp op);
};

// Wraps an SSA name in a new SSA name. This is used for correctness while
// constructing SSA, and is removed immediately after the initial SSA is built.
class MCopy : public MUnaryInstruction
{
    MCopy(MDefinition *ins)
      : MUnaryInstruction(ins)
    {
        setResultType(ins->type());
    }

  public:
    INSTRUCTION_HEADER(Copy);
    static MCopy *New(MDefinition *ins);

    HashNumber valueHash() const;
    bool congruentTo(MDefinition * const &ins) const;
};

// Takes a typed value and returns an untyped value.
class MBox : public MUnaryInstruction
{
    MBox(MDefinition *ins)
      : MUnaryInstruction(ins)
    {
        setIdempotent();
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Box);
    static MBox *New(MDefinition *ins)
    {
        // Cannot box a box.
        JS_ASSERT(ins->type() != MIRType_Value);

        return new MBox(ins);
    }
};

// Takes a typed value and checks if it is a certain type. If so, the payload
// is unpacked and returned as that type. Otherwise, it is considered a
// deoptimization.
class MUnbox : public MUnaryInstruction
{
    MUnbox(MDefinition *ins, MIRType type)
      : MUnaryInstruction(ins)
    {
        JS_ASSERT(ins->type() == MIRType_Value);
        setResultType(type);
        setIdempotent();
    }

  public:
    INSTRUCTION_HEADER(Unbox);
    static MUnbox *New(MDefinition *ins, MIRType type)
    {
        return new MUnbox(ins, type);
    }
};

// Converts a primitive (either typed or untyped) to a double. If the input is
// not primitive at runtime, a bailout occurs.
class MToDouble : public MUnaryInstruction
{
    MToDouble(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Double);
        setIdempotent();
    }

  public:
    INSTRUCTION_HEADER(ToDouble);
    static MToDouble *New(MDefinition *def)
    {
        return new MToDouble(def);
    }

    MDefinition *input() const {
        return getOperand(0);
    }
};

// Converts a primitive (either typed or untyped) to an int32. If the input is
// not primitive at runtime, a bailout occurs. If the input cannot be converted
// to an int32 without loss (i.e. "5.5" or undefined) then a bailout occurs.
class MToInt32 : public MUnaryInstruction
{
    MToInt32(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Int32);
        setIdempotent();
    }

  public:
    INSTRUCTION_HEADER(ToInt32);
    static MToInt32 *New(MDefinition *def)
    {
        return new MToInt32(def);
    }

    MDefinition *input() const {
        return getOperand(0);
    }
};

// Converts a value or typed input to a truncated int32, for use with bitwise
// operations. This is an infallible ValueToECMAInt32.
class MTruncateToInt32 : public MUnaryInstruction
{
    MTruncateToInt32(MDefinition *def)
      : MUnaryInstruction(def)
    {
        setResultType(MIRType_Int32);
        setIdempotent();
    }

  public:
    INSTRUCTION_HEADER(TruncateToInt32);
    static MTruncateToInt32 *New(MDefinition *def)
    {
        return new MTruncateToInt32(def);
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
    }

  public:
    INSTRUCTION_HEADER(BitNot);
    static MBitNot *New(MDefinition *input);

    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(bool useValueNumbers);
    void infer(const TypeOracle::Unary &u);
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
    }

  public:
    TypePolicy *typePolicy() {
        return this;
    }

    MDefinition *foldsTo(bool useValueNumbers);
    virtual MDefinition *foldIfZero(size_t operand) = 0;
    virtual MDefinition *foldIfEqual()  = 0;
    void infer(const TypeOracle::Binary &b);
};

class MBitAnd : public MBinaryBitwiseInstruction
{
    MBitAnd(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitAnd);
    static MBitAnd *New(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(operand); // 0 & x => 0;
    }

    MDefinition *foldIfEqual() {
        return getOperand(0); // x & x => x;
    }
};

class MBitOr : public MBinaryBitwiseInstruction
{
    MBitOr(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(BitOr);
    static MBitOr *New(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(1 - operand); // 0 | x => x, so if ith is 0, return (1-i)th
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
    INSTRUCTION_HEADER(BitXor);
    static MBitXor *New(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        return getOperand(1 - operand); // 0 ^ x => x
    }

    MDefinition *foldIfEqual() {
        return MConstant::New(Int32Value(0));
    }
};

class MLsh : public MBinaryBitwiseInstruction
{
    MLsh(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(Lsh);
    static MLsh *New(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 << x => 0
        // x << 0 => x
        return getOperand(0);
    }

    MDefinition *foldIfEqual() {
        return this;
    }
};

class MRsh : public MBinaryBitwiseInstruction
{
    MRsh(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right)
    { }

  public:
    INSTRUCTION_HEADER(Rsh);
    static MRsh *New(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 >> x => 0
        // x >> 0 => x
        return getOperand(0);
    }

    MDefinition *foldIfEqual() {
        return this;
    }
};

class MUrsh : public MBinaryBitwiseInstruction
{
    bool canOverflow_;

    MUrsh(MDefinition *left, MDefinition *right)
      : MBinaryBitwiseInstruction(left, right),
        canOverflow_(true)
    { }

  public:
    INSTRUCTION_HEADER(Ursh);
    static MUrsh *New(MDefinition *left, MDefinition *right);

    MDefinition *foldIfZero(size_t operand) {
        // 0 >>> x => 0
        if (operand == 0)
            return getOperand(0);

        return this;
    }

    MDefinition *foldIfEqual() {
        return this;
    }

    bool canOverflow() {
        // solution is only negative when lhs < 0 and rhs & 0x1f == 0
        MDefinition *lhs = getOperand(0);
        MDefinition *rhs = getOperand(1);

        if (lhs->isConstant() && lhs->toConstant()->value().toInt32() >= 0)
            return false;

        if (rhs->isConstant() && (rhs->toConstant()->value().toInt32() & 0x1F) != 0)
            return false;

        return canOverflow_;
    }

    bool fallible() {
        return canOverflow();
    }
};

class MBinaryArithInstruction
  : public MBinaryInstruction,
    public BinaryArithPolicy
{
  public:
    MBinaryArithInstruction(MDefinition *left, MDefinition *right)
      : MBinaryInstruction(left, right)
    { }

    TypePolicy *typePolicy() {
        return this;
    }
    MIRType specialization() const {
        return specialization_;
    }

    MDefinition *foldsTo(bool useValueNumbers);

    virtual double getIdentity() = 0;

    void infer(const TypeOracle::Binary &b);
};

class MAdd : public MBinaryArithInstruction
{
    MAdd(MDefinition *left, MDefinition *right)
      : MBinaryArithInstruction(left, right)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Add);
    static MAdd *New(MDefinition *left, MDefinition *right) {
        return new MAdd(left, right);
    }

    double getIdentity() {
        return 0;
    }
};

class MMul : public MBinaryArithInstruction
{
    bool canOverflow_;
    bool canBeNegativeZero_;

    MMul(MDefinition *left, MDefinition *right)
      : MBinaryArithInstruction(left, right),
        canOverflow_(true),
        canBeNegativeZero_(true)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Mul);
    static MMul *New(MDefinition *left, MDefinition *right) {
        return new MMul(left, right);
    }

    double getIdentity() {
        return 1;
    }

    bool canOverflow() {
        return canOverflow_;
    }

    bool canBeNegativeZero() {
        return canBeNegativeZero_;
    }

    bool fallible() {
        return canBeNegativeZero_ || canOverflow_;
    }
};

class MPhi : public MDefinition, public InlineForwardListNode<MPhi>
{
    js::Vector<MDefinition *, 2, IonAllocPolicy> inputs_;
    uint32 slot_;
    bool triedToSpecialize_;

    MPhi(uint32 slot)
      : slot_(slot),
        triedToSpecialize_(false)
    {
        setResultType(MIRType_Value);
    }

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        inputs_[index] = operand;
    }

  public:
    INSTRUCTION_HEADER(Phi);
    static MPhi *New(uint32 slot);

    MDefinition *getOperand(size_t index) const {
        return inputs_[index];
    }
    size_t numOperands() const {
        return inputs_.length();
    }
    uint32 slot() const {
        return slot_;
    }
    bool triedToSpecialize() const {
        return triedToSpecialize_;
    }
    void specialize(MIRType type) {
        triedToSpecialize_ = true;
        setResultType(type);
    }
    bool addInput(MDefinition *ins);

    MDefinition *foldsTo(bool useValueNumbers);

    bool congruentTo(MDefinition * const &ins) const;
};

// A snapshot contains the information needed to reconstruct the interpreter
// state from a position in the JIT. See the big comment near snapshot() in
// IonBuilder.cpp.
class MSnapshot : public MNode
{
    friend class MBasicBlock;

    MDefinition **operands_;
    uint32 stackDepth_;
    jsbytecode *pc_;

    MSnapshot(MBasicBlock *block, jsbytecode *pc);
    bool init(MBasicBlock *state);
    void inherit(MBasicBlock *state);

  protected:
    void setOperand(size_t index, MDefinition *operand) {
        JS_ASSERT(index < stackDepth_);
        operands_[index] = operand;
    }

  public:
    static MSnapshot *New(MBasicBlock *block, jsbytecode *pc);

    MNode::Kind kind() const {
        return MNode::Snapshot;
    }
    size_t numOperands() const {
        return stackDepth_;
    }
    MDefinition *getOperand(size_t index) const {
        JS_ASSERT(index < stackDepth_);
        return operands_[index];
    }
    jsbytecode *pc() const {
        return pc_;
    }
    uint32 stackDepth() const {
        return stackDepth_;
    }
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

MSnapshot *MNode::toSnapshot()
{
    JS_ASSERT(isSnapshot());
    return (MSnapshot *)this;
}

MInstruction *MDefinition::toInstruction()
{
    JS_ASSERT(!isPhi());
    return (MInstruction *)this;
}

void MNode::initOperand(size_t index, MDefinition *ins)
{
    setOperand(index, ins);
    ins->addUse(this, index);
}

} // namespace ion
} // namespace js

#endif // jsion_mir_h__

