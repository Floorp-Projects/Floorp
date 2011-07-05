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
#include "IonAllocPolicy.h"
#include "InlineList.h"
#include "MOpcodes.h"
#include "FixedArityList.h"

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

class MInstruction;
class MBasicBlock;
class MUse;
class MIRGraph;

// Represents a use of a definition.
class MUse : public TempObject
{
    friend class MInstruction;

    MUse *next_;            // Next use in the use chain.
    MInstruction *ins_;     // The instruction that is using this operand.
    uint32 index_;          // The index of this operand in its owner.

    MUse(MUse *next, MInstruction *owner, uint32 index)
      : next_(next), ins_(owner), index_(index)
    { }

    void setNext(MUse *next) {
        next_ = next;
    }

  public:
    static inline MUse *New(MUse *next, MInstruction *owner, uint32 index)
    {
        return new MUse(next, owner, index);
    }

    MInstruction *ins() const {
        return ins_;
    }
    uint32 index() const {
        return index_;
    }
    MUse *next() const {
        return next_;
    }
};

class MUseIterator
{
    MInstruction *def;
    MUse *use;
    MUse *prev_;

  public:
    inline MUseIterator(MInstruction *def);

    bool more() const {
        return !!use;
    }
    void next() {
        if (use) {
            prev_ = use;
            use = use->next();
        }
    }
    MUse * operator ->() const {
        return use;
    }
    MUse * operator *() const {
        return use;
    }
    MUse *prev() const {
        return prev_;
    }

    // Unlink the current entry from the use chain, advancing to the next
    // instruction at the same time. The old use node is returned.
    inline MUse *unlink();
};

class MOperand : public TempObject
{
    friend class MInstruction;

    MInstruction *ins_;

    MOperand(MInstruction *ins)
      : ins_(ins)
    { }

    void setInstruction(MInstruction *ins) {
        ins_ = ins;
    }

  public:
    static MOperand *New(MInstruction *ins)
    {
        return new MOperand(ins);
    }

    MInstruction *ins() const {
        return ins_;
    }
};

// An MInstruction is an SSA name and definition. It has an opcode, a list of
// operands, and a list of its uses by subsequent instructions.
class MInstruction
  : public TempObject,
    public InlineListNode<MInstruction>
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
    MBasicBlock *block_;    // Containing basic block.
    MUse *uses_;            // Use chain.
    uint32 id_;             // Ordered id for register allocation.
    MIRType resultType_;    // Actual result type.
    uint32 usedTypes_;      // Set of used types.
    uint32 flags_;          // Bit flags.

  private:
    static const uint32 IN_WORKLIST =  0x01;
    static const uint32 REWRITES_DEF = 0x02;
    static const uint32 EMIT_AT_USES = 0x04;
    static const uint32 LOOP_INVARIANT = 0x08;

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
    MInstruction()
      : block_(NULL),
        uses_(NULL),
        id_(0),
        resultType_(MIRType_None),
        usedTypes_(0),
        flags_(0)
    { }

    virtual Opcode op() const = 0;
    void printName(FILE *fp);
    virtual void printOpcode(FILE *fp);

    uint32 id() const {
        JS_ASSERT(block_);
        return id_;
    }
    void setId(uint32 id) {
        id_ = id;
    }
    bool inWorklist() const {
        return hasFlags(IN_WORKLIST);
    }
    void setInWorklist() {
        JS_ASSERT(!inWorklist());
        setFlags(IN_WORKLIST);
    }
    void setInWorklistUnchecked() {
        setFlags(IN_WORKLIST);
    }
    void setNotInWorklist() {
        JS_ASSERT(inWorklist());
        removeFlags(IN_WORKLIST);
    }

    void setLoopInvariant() {
        setFlags(LOOP_INVARIANT);
    }

    void setNotLoopInvariant() {
        removeFlags(LOOP_INVARIANT);
    }

    bool isLoopInvariant() {
        return hasFlags(LOOP_INVARIANT);
    }

    MBasicBlock *block() const {
        JS_ASSERT(block_);
        return block_;
    }
    MIRType type() const {
        return resultType_;
    }

    // Returns a normalized type this instruction should be used as. If exactly
    // one type was requested, then that type is returned. If more than one
    // type was requested, then Value is returned.
    MIRType usedAsType() const;

    // Returns this instruction's use chain.
    MUse *uses() const {
        return uses_;
    }

    // Removes a use, returning the next use in the use list.
    MUse *removeUse(MUse *prev, MUse *use);

    // Number of uses of this instruction.
    size_t useCount() const;

    // Returns the instruction at a given operand.
    virtual MOperand *getOperand(size_t index) const = 0;
    virtual size_t numOperands() const = 0;
    inline MInstruction *getInput(size_t index) const {
        return getOperand(index)->ins();
    }

    // Returns the input type this instruction expects.
    virtual MIRType requiredInputType(size_t index) const {
        return MIRType_None;
    }

    // Allows an instruction to despecialize if its input types are too complex
    // to handle. Returns false on no change, true if the specialization was
    // downgraded.
    virtual bool adjustForInputs() {
        return false;
    }

    // Certain instructions can rewrite definitions in dominated snapshots. For
    // example:
    //   0: getprop
    //   1: snapshot (slot0 = 0)
    //   2: unbox(0, int32)
    //   3: snapshot (slot0 = 0)
    //
    // In this case, we would like to overwrite the second, but not first,
    // snapshot's slot0 mapping with the unboxed value. It is difficult to make
    // this work in the same pass as inserting instructions, so we annotate it
    // for a second pass.
    bool rewritesDef() const {
        return hasFlags(REWRITES_DEF);
    }
    void setRewritesDef() {
        setFlags(REWRITES_DEF);
    }
    virtual MInstruction *rewrittenDef() const {
        JS_NOT_REACHED("Opcodes which can rewrite defs must implement this.");
        return NULL;
    }
    bool emitAtUses() const {
        return hasFlags(EMIT_AT_USES);
    }
    void setEmitAtUses() {
        setFlags(EMIT_AT_USES);
    }

    // Replaces an operand, taking care to update use chains. No memory is
    // allocated; the existing data structures are re-linked.
    void replaceOperand(MUse *prev, MUse *use, MInstruction *ins);
    void replaceOperand(MUseIterator &use, MInstruction *ins);
    void replaceOperand(size_t index, MInstruction *ins);

    void addUse(MInstruction *ins, size_t index) {
        uses_ = MUse::New(uses_, ins, index);
    }

    // Adds a use from a node that is being recycled during operand
    // replacement.
    void addUse(MUse *use) {
        JS_ASSERT(use->ins()->getInput(use->index()) == this);
        use->setNext(uses_);
        uses_ = use;
    }

  public:   // Functions for analysis phases.
    // Analyzes inputs and uses and updates type information. If type
    // information changed, returns true, otherwise, returns false.
    bool addUsedTypes(uint32 types) {
        uint32 newTypes = types | usedTypes();
        if (newTypes == usedTypes())
            return false;
        usedTypes_ = newTypes;
        return true;
    }
    bool useAsType(MIRType type) {
        JS_ASSERT(type < MIRType_Value);
        return addUsedTypes(1 << uint32(type));
    }
    uint32 usedTypes() const {
        return usedTypes_;
    }

    // Asks a typed instruction to specialize itself to a specific type. If
    // this is not possible, or not desireable, then the caller must insert an
    // unbox operation.
    virtual bool specializeTo(MIRType type) {
        return false;
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

    virtual bool accept(MInstructionVisitor *visitor) = 0;

  protected:
    // Sets a raw operand; instruction implementation dependent.
    virtual void setOperand(size_t index, MOperand *operand) = 0;
    void setOperand(size_t index, MInstruction *ins);

    // Initializes an operand for the first time.
    void initOperand(size_t index, MInstruction *ins) {
        setOperand(index, MOperand::New(ins));
        ins->addUse(this, index);
    }

    void setResultType(MIRType type) {
        resultType_ = type;
    }
};

#define INSTRUCTION_HEADER(opcode)                                          \
    Opcode op() const {                                                     \
        return MInstruction::Op_##opcode;                                   \
    }                                                                       \
    bool accept(MInstructionVisitor *visitor) {                             \
        return visitor->visit##opcode(this);                                \
    }

template <size_t Arity>
class MAryInstruction : public MInstruction
{
  protected:
    FixedArityList<MOperand *, Arity> operands_;

    void setOperand(size_t index, MOperand *operand) {
        operands_[index] = operand;
    }

  public:
    MOperand *getOperand(size_t index) const {
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
    void printOpcode(FILE *fp);
};

// A reference to a formal parameter.
class MParameter : public MAryInstruction<0>
{
    int32 index_;

  public:
    static const int32 CALLEE_SLOT = -2;
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
};

class MControlInstruction : public MInstruction
{
  protected:
    MBasicBlock *successors[2];

  public:
    MControlInstruction()
      : successors()
    { }

    uint32 numSuccessors() const {
        if (successors[1])
            return 2;
        if (successors[0])
            return 1;
        return 0;
    }

    MBasicBlock *getSuccessor(uint32 i) const {
        JS_ASSERT(i < numSuccessors());
        return successors[i];
    }

    void replaceSuccessor(size_t i, MBasicBlock *split) {
        JS_ASSERT(successors[i]);
        successors[i] = split;
    }
};

template <size_t Arity>
class MAryControlInstruction : public MControlInstruction
{
    FixedArityList<MOperand *, Arity> operands_;

  protected:
    void setOperand(size_t index, MOperand *operand) {
        operands_[index] = operand;
    }

  public:
    MOperand *getOperand(size_t index) const {
        return operands_[index];
    }
    size_t numOperands() const {
        return Arity;
    }
};

// Jump to the start of another basic block.
class MGoto : public MAryControlInstruction<0>
{
    MGoto(MBasicBlock *target) {
        successors[0] = target;
    }

  public:
    INSTRUCTION_HEADER(Goto);
    static MGoto *New(MBasicBlock *target);

    MBasicBlock *target() {
        return successors[0];
    }
};

// Tests if the input instruction evaluates to true or false, and jumps to the
// start of a corresponding basic block.
class MTest : public MAryControlInstruction<1>
{
    MTest(MInstruction *ins, MBasicBlock *if_true, MBasicBlock *if_false)
    {
        successors[0] = if_true;
        successors[1] = if_false;
        initOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(Test);
    static MTest *New(MInstruction *ins,
                      MBasicBlock *ifTrue, MBasicBlock *ifFalse);

    MIRType requiredInputType(size_t index) const {
        return MIRType_Any;
    }

    MBasicBlock *ifTrue() const {
        return getSuccessor(0);
    }
    MBasicBlock *ifFalse() const {
        return getSuccessor(1);
    }
};

// Returns from this function to the previous caller.
class MReturn : public MAryControlInstruction<1>
{
    MReturn(MInstruction *ins)
    {
        initOperand(0, ins);
    }

  public:
    INSTRUCTION_HEADER(Return);
    static MReturn *New(MInstruction *ins);

    MIRType requiredInputType(size_t index) const {
        return MIRType_Value;
    }
};

class MUnaryInstruction : public MAryInstruction<1>
{
  protected:
    MUnaryInstruction(MInstruction *ins)
    {
        initOperand(0, ins);
    }
};

class MBinaryInstruction : public MAryInstruction<2>
{
    // Specifies three levels of specialization:
    //  - < Value. This input is expected and required.
    //  - == Value. Try to convert to the expected return type.
    //  - == None. This op should not be specialized.
    MIRType specialization_;

  protected:
    MBinaryInstruction(MInstruction *left, MInstruction *right)
      : specialization_(MIRType_None)
    {
        initOperand(0, left);
        initOperand(1, right);
    }

    MIRType specialization() const {
        return specialization_;
    }

  public:
    void infer(const TypeOracle::Binary &b);
    bool adjustForInputs();
};

// Wraps an SSA name in a new SSA name. This is used for correctness while
// constructing SSA, and is removed immediately after the initial SSA is built.
class MCopy : public MUnaryInstruction
{
    MCopy(MInstruction *ins)
      : MUnaryInstruction(ins)
    {
        setResultType(ins->type());
    }

  public:
    INSTRUCTION_HEADER(Copy);
    static MCopy *New(MInstruction *ins);

    MIRType requiredInputType(size_t index) const {
        return MIRType_Any;
    }
};

// Takes a typed value and returns an untyped value.
class MBox : public MUnaryInstruction
{
    MBox(MInstruction *ins)
      : MUnaryInstruction(ins)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Box);
    static MBox *New(MInstruction *ins)
    {
        // Cannot box a box.
        JS_ASSERT(ins->type() != MIRType_Value);

        return new MBox(ins);
    }

    MIRType requiredInputType(size_t index) const {
        return MIRType_Any;
    }
};

// Takes a typed value and checks if it is a certain type. If so, the payload
// is unpacked and returned as that type. Otherwise, it is considered a
// deoptimization.
class MUnbox : public MUnaryInstruction
{
    MUnbox(MInstruction *ins, MIRType type)
      : MUnaryInstruction(ins)
    {
        JS_ASSERT(ins->type() == MIRType_Value);
        setResultType(type);
    }

  public:
    INSTRUCTION_HEADER(Unbox);
    static MUnbox *New(MInstruction *ins, MIRType type)
    {
        return new MUnbox(ins, type);
    }

    MIRType requiredInputType(size_t index) const {
        return MIRType_Value;
    }

    MInstruction *rewrittenDef() const {
        return getInput(0);
    }
};

class MBitAnd : public MBinaryInstruction
{
    MBitAnd(MInstruction *left, MInstruction *right)
      : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Int32);
    }

  public:
    INSTRUCTION_HEADER(BitAnd);
    static MBitAnd *New(MInstruction *left, MInstruction *right);

    MIRType requiredInputType(size_t index) const {
        return specialization();
    }
};

class MAdd : public MBinaryInstruction
{
    MAdd(MInstruction *left, MInstruction *right)
      : MBinaryInstruction(left, right)
    {
        setResultType(MIRType_Value);
    }

  public:
    INSTRUCTION_HEADER(Add);
    static MAdd *New(MInstruction *left, MInstruction *right) {
        return new MAdd(left, right);
    }

    MIRType requiredInputType(size_t index) const {
        return specialization();
    }
};

class MPhi : public MInstruction
{
    js::Vector<MOperand *, 2, IonAllocPolicy> inputs_;
    uint32 slot_;

    MPhi(uint32 slot)
      : slot_(slot)
    {
        setResultType(MIRType_Value);
    }

  protected:
    void setOperand(size_t index, MOperand *operand) {
        inputs_[index] = operand;
    }

  public:
    INSTRUCTION_HEADER(Phi);
    static MPhi *New(uint32 slot);

    MOperand *getOperand(size_t index) const {
        return inputs_[index];
    }
    size_t numOperands() const {
        return inputs_.length();
    }
    uint32 slot() const {
        return slot_;
    }
    bool addInput(MInstruction *ins);
    MIRType requiredInputType(size_t index) const {
        return MIRType_Value;
    }
};

// A snapshot contains the information needed to reconstruct the interpreter
// state from a position in the JIT. See the big comment near snapshot() in
// IonBuilder.cpp.
class MSnapshot : public MInstruction
{
    friend class MBasicBlock;

    MOperand **operands_;
    uint32 stackDepth_;
    jsbytecode *pc_;

    MSnapshot(MBasicBlock *block, jsbytecode *pc);
    bool init(MBasicBlock *state);
    void inherit(MBasicBlock *state);

  protected:
    void setOperand(size_t index, MOperand *operand) {
        JS_ASSERT(index < stackDepth_);
        operands_[index] = operand;
    }

  public:
    INSTRUCTION_HEADER(Snapshot);
    static MSnapshot *New(MBasicBlock *block, jsbytecode *pc);

    void printOpcode(FILE *fp);
    MIRType requiredInputType(size_t index) const {
        return MIRType_Any;
    }

    size_t numOperands() const {
        return stackDepth_;
    }
    MOperand *getOperand(size_t index) const {
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

inline
MUseIterator::MUseIterator(MInstruction *def)
  : def(def),
    use(def->uses()),
    prev_(NULL)
{
}

inline MUse *
MUseIterator::unlink()
{
    MUse *old = use;
    use = def->removeUse(prev(), use);
    return old;
}

// Implement opcode casts now that the compiler can see the inheritance.
#define OPCODE_CASTS(opcode)                                                \
    M##opcode *MInstruction::to##opcode()                                   \
    {                                                                       \
        JS_ASSERT(is##opcode());                                            \
        return static_cast<M##opcode *>(this);                              \
    }
MIR_OPCODE_LIST(OPCODE_CASTS)
#undef OPCODE_CASTS

} // namespace ion
} // namespace js

#endif // jsion_mir_h__

