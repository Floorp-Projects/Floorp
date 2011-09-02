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

#ifndef jsion_lir_h__
#define jsion_lir_h__

// This file declares the core data structures for LIR: storage allocations for
// inputs and outputs, as well as the interface instructions must conform to.

#include "jscntxt.h"
#include "IonAllocPolicy.h"
#include "InlineList.h"
#include "FixedArityList.h"
#include "LOpcodes.h"
#include "TypeOracle.h"
#include "IonRegisters.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "shared/Assembler-shared.h"
#include "Snapshots.h"
#include "Bailouts.h"

#if defined(JS_CPU_X86)
# include "x86/StackAssignment-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/StackAssignment-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/StackAssignment-arm.h"
#else
#error "CPU Not Supported"
#endif

namespace js {
namespace ion {

class LUse;
class LGeneralReg;
class LFloatReg;
class LStackSlot;
class LArgument;
class LConstantIndex;
class MBasicBlock;
class MTableSwitch;
class MIRGenerator;
class MSnapshot;

static const uint32 MAX_VIRTUAL_REGISTERS = (1 << 21) - 1;
static const uint32 VREG_INCREMENT = 1;

static const uint32 THIS_FRAME_SLOT = 0;

#if defined(JS_NUNBOX32)
# define BOX_PIECES         2
static const uint32 VREG_TYPE_OFFSET = 0;
static const uint32 VREG_DATA_OFFSET = 1;
static const uint32 TYPE_INDEX = 0;
static const uint32 PAYLOAD_INDEX = 1;
#elif defined(JS_PUNBOX64)
# define BOX_PIECES         1
#else
# error "Unknown!"
#endif

// Represents storage for an operand. For constants, the pointer is tagged
// with a single bit, and the untagged pointer is a pointer to a Value.
class LAllocation : public TempObject
{
    uintptr_t bits_;

  protected:
    static const uintptr_t TAG_BIT = 1;
    static const uintptr_t TAG_SHIFT = 0;
    static const uintptr_t TAG_MASK = 1 << TAG_SHIFT;
    static const uintptr_t KIND_BITS = 3;
    static const uintptr_t KIND_SHIFT = TAG_SHIFT + TAG_BIT;
    static const uintptr_t KIND_MASK = (1 << KIND_BITS) - 1;
    static const uintptr_t DATA_BITS = (sizeof(uint32) * 8) - KIND_BITS - TAG_BIT;
    static const uintptr_t DATA_SHIFT = KIND_SHIFT + KIND_BITS;
    static const uintptr_t DATA_MASK = (1 << DATA_BITS) - 1;

  public:
    enum Kind {
        USE,            // Use of a virtual register, with physical allocation policy.
        CONSTANT_VALUE, // Constant js::Value.
        CONSTANT_INDEX, // Constant arbitrary index.
        GPR,            // General purpose register.
        FPU,            // Floating-point register.
        STACK_SLOT,     // 32-bit stack slot.
        DOUBLE_SLOT,    // 64-bit stack slot.
        ARGUMENT        // Argument slot.
    };

  protected:
    bool isTagged() const {
        return !!(bits_ & TAG_MASK);
    }

    int32 data() const {
        return int32(bits_) >> DATA_SHIFT;
    }
    void setData(int32 data) {
        JS_ASSERT(int32(data) <= int32(DATA_MASK));
        bits_ &= ~(DATA_MASK << DATA_SHIFT);
        bits_ |= (data << DATA_SHIFT);
    }
    void setKindAndData(Kind kind, uint32 data) {
        JS_ASSERT(int32(data) <= int32(DATA_MASK));
        bits_ = (uint32(kind) << KIND_SHIFT) | data << DATA_SHIFT;
    }

    LAllocation(Kind kind, uint32 data) {
        setKindAndData(kind, data);
    }
    explicit LAllocation(Kind kind) {
        setKindAndData(kind, 0);
    }

  public:
    LAllocation() : bits_(0)
    { }

    static LAllocation *New() {
        return new LAllocation();
    }
    template <typename T>
    static LAllocation *New(const T &other) {
        return new LAllocation(other);
    }

    // The value pointer must be rooted in MIR and have its low bit cleared.
    explicit LAllocation(const Value *vp) {
        bits_ = uintptr_t(vp);
        JS_ASSERT(!isTagged());
        bits_ |= TAG_MASK;
    }
    inline explicit LAllocation(const AnyRegister &reg);

    Kind kind() const {
        if (isTagged())
            return CONSTANT_VALUE;
        return (Kind)((bits_ >> KIND_SHIFT) & KIND_MASK);
    }

    bool isUse() const {
        return kind() == USE;
    }
    bool isConstant() const {
        return isConstantValue() || isConstantIndex();
    }
    bool isConstantValue() const {
        return kind() == CONSTANT_VALUE;
    }
    bool isConstantIndex() const {
        return kind() == CONSTANT_INDEX;
    }
    bool isValue() const {
        return kind() == CONSTANT_VALUE;
    }
    bool isGeneralReg() const {
        return kind() == GPR;
    }
    bool isFloatReg() const {
        return kind() == FPU;
    }
    bool isStackSlot() const {
        return kind() == STACK_SLOT || kind() == DOUBLE_SLOT;
    }
    bool isArgument() const {
        return kind() == ARGUMENT;
    }
    bool isRegister() const {
        return isGeneralReg() || isFloatReg();
    }
    bool isMemory() const {
        return isStackSlot() || isArgument();
    }
    bool isDouble() const {
        return kind() == DOUBLE_SLOT || kind() == FPU;
    }
    inline LUse *toUse();
    inline const LUse *toUse() const;
    inline const LGeneralReg *toGeneralReg() const;
    inline const LFloatReg *toFloatReg() const;
    inline const LStackSlot *toStackSlot() const;
    inline const LArgument *toArgument() const;
    inline const LConstantIndex *toConstantIndex() const;
    inline AnyRegister toRegister() const;

    const Value *toConstant() const {
        JS_ASSERT(isConstantValue());
        return reinterpret_cast<const Value *>(bits_ & ~TAG_MASK);
    }

    bool operator ==(const LAllocation &other) const {
        return bits_ == other.bits_;
    }

    bool operator !=(const LAllocation &other) const {
        return bits_ != other.bits_;
    }

    static void PrintAllocation(FILE *fp, const LAllocation *a);
};

class LUse : public LAllocation
{
    static const uint32 POLICY_BITS = 3;
    static const uint32 POLICY_SHIFT = 0;
    static const uint32 POLICY_MASK = (1 << POLICY_BITS) - 1;
    static const uint32 REG_BITS = 5;
    static const uint32 REG_SHIFT = POLICY_SHIFT + POLICY_BITS;
    static const uint32 REG_MASK = (1 << REG_BITS) - 1;

    // Virtual registers get the remaining 21 bits.
    static const uint32 VREG_BITS = DATA_BITS - (REG_SHIFT + REG_BITS);
    static const uint32 VREG_SHIFT = REG_SHIFT + REG_BITS;
    static const uint32 VREG_MASK = (1 << VREG_BITS) - 1;

  public:
    enum Policy {
        // Input should be in a read-only register or stack slot.
        ANY,

        // Input must be in a read-only register.
        REGISTER,

        // Input must be in a specific, read-only register.
        FIXED,

        // Keep the used virtual register alive, and use whatever allocation is
        // available. This is similar to ANY but hints to the register allocator
        // that it is never useful to optimize this site.
        KEEPALIVE,

        // Input should be in a writable register.
        COPY                
    };

    void set(Policy policy, uint32 reg) {
        setKindAndData(USE, (policy << POLICY_SHIFT) | (reg << REG_SHIFT));
    }

  public:
    LUse(uint32 vreg, Policy policy) {
        set(policy, 0);
        setVirtualRegister(vreg);
    }
    LUse(Policy policy) {
        set(policy, 0);
    }
    LUse(Register reg) {
        set(FIXED, reg.code());
    }
    LUse(FloatRegister reg) {
        set(FIXED, reg.code());
    }

    void setVirtualRegister(uint32 index) {
        JS_STATIC_ASSERT(VREG_MASK <= MAX_VIRTUAL_REGISTERS);
        JS_ASSERT(index < VREG_MASK);

        uint32 old = data() & ~(VREG_MASK << VREG_SHIFT);
        setData(old | (index << VREG_SHIFT));
    }

    Policy policy() const {
        Policy policy = (Policy)((data() >> POLICY_SHIFT) & POLICY_MASK);
        return policy;
    }
    uint32 virtualRegister() const {
        uint32 index = (data() >> VREG_SHIFT) & VREG_MASK;
        return index;
    }
    uint32 registerCode() const {
        JS_ASSERT(policy() == FIXED);
        return (data() >> REG_SHIFT) & REG_MASK;
    }
    bool isFixedRegister() const {
        return policy() == FIXED;
    }
};

class LGeneralReg : public LAllocation
{
  public:
    explicit LGeneralReg(Register reg)
      : LAllocation(GPR, reg.code())
    { }

    Register reg() const {
        return Register::FromCode(data());
    }
};

class LFloatReg : public LAllocation
{
  public:
    explicit LFloatReg(FloatRegister reg)
      : LAllocation(FPU, reg.code())
    { }

    FloatRegister reg() const {
        return FloatRegister::FromCode(data());
    }
};

// Arbitrary constant index.
class LConstantIndex : public LAllocation
{
    explicit LConstantIndex(uint32 index)
      : LAllocation(CONSTANT_INDEX, index)
    { }

  public:
    // Used as a placeholder for inputs that can be ignored.
    static LConstantIndex Bogus() {
        return LConstantIndex(0);
    }

    static LConstantIndex FromIndex(uint32 index) {
        return LConstantIndex(index);
    }

    uint32 index() const {
        return data();
    }
};

// Stack slots are indexes into the stack, given that each slot is size
// STACK_SLOT_SIZE.
class LStackSlot : public LAllocation
{
  public:
    explicit LStackSlot(uint32 slot, bool isDouble = false)
      : LAllocation(isDouble ? DOUBLE_SLOT : STACK_SLOT, slot)
    { }

    bool isDouble() const {
        return kind() == DOUBLE_SLOT;
    }

    uint32 slot() const {
        return data();
    }
};

// Arguments are reverse indexes into the stack, and like LStackSlot, each
// index is measured in increments of STACK_SLOT_SIZE.
class LArgument : public LAllocation
{
  public:
    explicit LArgument(int32 index)
      : LAllocation(ARGUMENT, index)
    { }

    int32 index() const {
        return data();
    }
};

// Represents storage for a definition.
class LDefinition
{
    // Bits containing policy, type, and virtual register.
    uint32 bits_;

    // Before register allocation, this optionally contains a fixed policy.
    // Register allocation assigns this field to a physical policy if none is
    // preset.
    //
    // Right now, pre-allocated outputs are limited to the following:
    //   * Physical argument stack slots.
    //   * Physical registers.
    LAllocation output_;

    static const uint32 TYPE_BITS = 3;
    static const uint32 TYPE_SHIFT = 0;
    static const uint32 TYPE_MASK = (1 << TYPE_BITS) - 1;
    static const uint32 POLICY_BITS = 2;
    static const uint32 POLICY_SHIFT = TYPE_SHIFT + TYPE_BITS;
    static const uint32 POLICY_MASK = (1 << POLICY_BITS) - 1;

    static const uint32 VREG_BITS = (sizeof(uint32) * 8) - (POLICY_BITS + TYPE_BITS);
    static const uint32 VREG_SHIFT = POLICY_SHIFT + POLICY_BITS;
    static const uint32 VREG_MASK = (1 << VREG_BITS) - 1;

  public:
    // Note that definitions, by default, are always allocated a register,
    // unless the policy specifies that an input can be re-used and that input
    // is a stack slot.
    enum Policy {
        // A random register of an appropriate class will be assigned.
        DEFAULT,

        // The policy is predetermined by the LAllocation attached to this
        // definition. The allocation may be:
        //   * A register, which may not appear as any fixed temporary.
        //   * A stack slot or argument.
        //
        // Register allocation will not modify a preset allocation.
        PRESET,

        // One definition per instruction must re-use the first input
        // allocation, which (for now) must be a register.
        MUST_REUSE_INPUT,

        // This definition's virtual register is the same as another; this is
        // for instructions which consume a register and silently define it as
        // the same register.
        REDEFINED
    };

    enum Type {
        INTEGER,    // Generic, integer data (GPR).
        POINTER,    // Generic, pointer-width data (GPR).
        OBJECT,     // Pointer that may be collected as garbage (GPR).
        DOUBLE,     // 64-bit point value (FPU).
        TYPE,       // Type tag, for nunbox systems.
        PAYLOAD,    // Payload, for nunbox systems.
        BOX         // Joined box, for punbox systems.
    };

    void set(uint32 index, Type type, Policy policy) {
        JS_STATIC_ASSERT(MAX_VIRTUAL_REGISTERS <= VREG_MASK);
        bits_ = (index << VREG_SHIFT) | (policy << POLICY_SHIFT) | (type << TYPE_SHIFT);
    }

  public:
    LDefinition(uint32 index, Type type, Policy policy = DEFAULT) {
        set(index, type, policy);
    }

    LDefinition(Type type, Policy policy = DEFAULT) {
        set(0, type, policy);
    }

    LDefinition(Type type, const LAllocation &a)
      : output_(a)
    {
        set(0, type, PRESET);
    }

    LDefinition() : bits_(0)
    { }

    static LDefinition BogusTemp() {
        return LDefinition(INTEGER, LConstantIndex::Bogus());
    }

    Policy policy() const {
        return (Policy)((bits_ >> POLICY_SHIFT) & POLICY_MASK);
    }
    Type type() const {
        return (Type)((bits_ >> TYPE_SHIFT) & TYPE_MASK);
    }
    uint32 virtualRegister() const {
        return (bits_ >> VREG_SHIFT) & VREG_MASK;
    }
    LAllocation *output() {
        return &output_;
    }
    const LAllocation *output() const {
        return &output_;
    }
    bool isPreset() const {
        return policy() == PRESET;
    }
    bool isBogusTemp() const {
        return isPreset() && output()->isConstantIndex();
    }
    void setVirtualRegister(uint32 index) {
        JS_ASSERT(index < VREG_MASK);
        bits_ &= ~(VREG_MASK << VREG_SHIFT);
        bits_ |= index << VREG_SHIFT;
    }
    void setOutput(const LAllocation &a) {
        output_ = a;
        if (!a.isUse()) {
            bits_ &= ~(POLICY_MASK << POLICY_SHIFT);
            bits_ |= PRESET << POLICY_SHIFT;
        }
    }

    static inline Type TypeFrom(MIRType type) {
        switch (type) {
          case MIRType_Boolean:
          case MIRType_Int32:
            return LDefinition::INTEGER;
          case MIRType_String:
          case MIRType_Object:
            return LDefinition::OBJECT;
          case MIRType_Double:
            return LDefinition::DOUBLE;
#if defined(JS_PUNBOX64)
          case MIRType_Value:
            return LDefinition::BOX;
#endif
          default:
            JS_NOT_REACHED("unexpected type");
            return LDefinition::BOX;
        }
    }
};

// Forward declarations of LIR types.
#define LIROP(op) class L##op;
    LIR_OPCODE_LIST(LIROP)
#undef LIROP

class LSnapshot;
class LInstructionVisitor;

class LInstruction : public TempObject,
                     public InlineListNode<LInstruction>
{
    uint32 id_;
    LSnapshot *snapshot_;

  protected:
    LInstruction()
      : id_(0),
        snapshot_(NULL)
    { }

  public:
    class InputIterator;
    enum Opcode {
#   define LIROP(name) LOp_##name,
        LIR_OPCODE_LIST(LIROP)
#   undef LIROP
        LOp_Invalid
    };

  public:
    virtual Opcode op() const = 0;

    // Returns the number of outputs of this instruction. If an output is
    // unallocated, it is an LDefinition, defining a virtual register.
    virtual size_t numDefs() const = 0;
    virtual LDefinition *getDef(size_t index) = 0;
    virtual void setDef(size_t index, const LDefinition &def) = 0;

    // Returns information about operands.
    virtual size_t numOperands() const = 0;
    virtual LAllocation *getOperand(size_t index) = 0;
    virtual void setOperand(size_t index, const LAllocation &a) = 0;

    // Returns information about temporary registers needed. Each temporary
    // register is an LUse with a TEMPORARY policy, or a fixed register.
    virtual size_t numTemps() const = 0;
    virtual LDefinition *getTemp(size_t index) = 0;
    virtual void setTemp(size_t index, const LDefinition &a) = 0;

    uint32 id() const {
        return id_;
    }
    void setId(uint32 id) {
        JS_ASSERT(!id_);
        JS_ASSERT(id);
        id_ = id;
    }
    LSnapshot *snapshot() const {
        return snapshot_;
    }
    void assignSnapshot(LSnapshot *snapshot);

    virtual void print(FILE *fp);
    virtual void printName(FILE *fp);
    virtual void printOperands(FILE *fp);
    virtual void printInfo(FILE *fp) {
    }

  public:
    // Opcode testing and casts.
#   define LIROP(name)                                                      \
    bool is##name() const {                                                 \
        return op() == LOp_##name;                                          \
    }                                                                       \
    inline L##name *to##name();
    LIR_OPCODE_LIST(LIROP)
#   undef LIROP

    virtual bool accept(LInstructionVisitor *visitor) = 0;
};

class LInstructionVisitor
{
  public:
#define VISIT_INS(op) virtual bool visit##op(L##op *) { JS_NOT_REACHED("implement " #op); return false; }
    LIR_OPCODE_LIST(VISIT_INS)
#undef VISIT_INS
};

typedef InlineList<LInstruction>::iterator LInstructionIterator;
typedef InlineList<LInstruction>::reverse_iterator LInstructionReverseIterator;

class LPhi;
class LBlock : public TempObject
{
    MBasicBlock *block_;
    Vector<LPhi *, 4, IonAllocPolicy> phis_;
    InlineList<LInstruction> instructions_;
    Label label_;

    LBlock(MBasicBlock *block)
      : block_(block)
    { }

  public:
    static LBlock *New(MBasicBlock *from) {
        return new LBlock(from);
    }
    void add(LInstruction *ins) {
        instructions_.pushBack(ins);
    }
    bool addPhi(LPhi *phi) {
        return phis_.append(phi);
    }
    size_t numPhis() const {
        return phis_.length();
    }
    LPhi *getPhi(size_t index) const {
        return phis_[index];
    }
    void removePhi(size_t index) {
        phis_.erase(&phis_[index]);
    }
    MBasicBlock *mir() const {
        return block_;
    }
    LInstructionIterator begin() {
        return instructions_.begin();
    }
    LInstructionIterator end() {
        return instructions_.end();
    }
    LInstructionReverseIterator rbegin() {
        return instructions_.rbegin();
    }
    LInstructionReverseIterator rend() {
        return instructions_.rend();
    }
    InlineList<LInstruction> &instructions() {
        return instructions_;
    }
    void insertAfter(LInstruction *at, LInstruction *ins) {
        instructions_.insertAfter(at, ins);
    }
    void insertBefore(LInstruction *at, LInstruction *ins) {
        instructions_.insertBefore(at, ins);
    }
    uint32 firstId();
    uint32 lastId();
    Label *label() {
        return &label_;
    }
};

template <size_t Defs, size_t Operands, size_t Temps>
class LInstructionHelper : public LInstruction
{
    FixedArityList<LDefinition, Defs> defs_;
    FixedArityList<LAllocation, Operands> operands_;
    FixedArityList<LDefinition, Temps> temps_;

  public:
    size_t numDefs() const {
        return Defs;
    }
    LDefinition *getDef(size_t index) {
        return &defs_[index];
    }
    size_t numOperands() const {
        return Operands;
    }
    LAllocation *getOperand(size_t index) {
        return &operands_[index];
    }
    size_t numTemps() const {
        return Temps;
    }
    LDefinition *getTemp(size_t index) {
        return &temps_[index];
    }

    void setDef(size_t index, const LDefinition &def) {
        defs_[index] = def;
    }
    void setOperand(size_t index, const LAllocation &a) {
        operands_[index] = a;
    }
    void setTemp(size_t index, const LDefinition &a) {
        temps_[index] = a;
    }

    virtual void printInfo(FILE *fp) {
        printOperands(fp);
    }
};

// An LSnapshot is a translation of an MSnapshot into LIR. Unlike MSnapshots,
// they cannot be shared, as they are filled in by the register allocator in
// order to capture the precise low-level stack state in between an
// instruction's input and output. During code generation, LSnapshots are
// compressed and saved in the compiled script.
class LSnapshot : public TempObject
{
  private:
    uint32 numSlots_;
    LAllocation *slots_;
    MResumePoint *mir_;
    SnapshotOffset snapshotOffset_;
    BailoutId bailoutId_;

    LSnapshot(MResumePoint *mir);
    bool init(MIRGenerator *gen);

  public:
    static LSnapshot *New(MIRGenerator *gen, MResumePoint *snapshot);

    size_t numEntries() const {
        return numSlots_;
    }
    size_t numSlots() const {
        return numSlots_ / BOX_PIECES;
    }
    LAllocation *payloadOfSlot(size_t i) {
        JS_ASSERT(i < numSlots());
        size_t entryIndex = (i * BOX_PIECES) + (BOX_PIECES - 1);
        return getEntry(entryIndex);
    }
#ifdef JS_NUNBOX32
    LAllocation *typeOfSlot(size_t i) {
        JS_ASSERT(i < numSlots());
        size_t entryIndex = (i * BOX_PIECES) + (BOX_PIECES - 2);
        return getEntry(entryIndex);
    }
#endif
    LAllocation *getEntry(size_t i) {
        JS_ASSERT(i < numSlots_);
        return &slots_[i];
    }
    void setEntry(size_t i, const LAllocation &alloc) {
        JS_ASSERT(i < numSlots_);
        slots_[i] = alloc;
    }
    MResumePoint *mir() const {
        return mir_;
    }
    SnapshotOffset snapshotOffset() const {
        return snapshotOffset_;
    }
    BailoutId bailoutId() const {
        return bailoutId_;
    }
    void setSnapshotOffset(SnapshotOffset offset) {
        JS_ASSERT(snapshotOffset_ == INVALID_SNAPSHOT_OFFSET);
        snapshotOffset_ = offset;
    }
    void setBailoutId(BailoutId id) {
        JS_ASSERT(bailoutId_ == INVALID_BAILOUT_ID);
        bailoutId_ = id;
    }
};

class LInstruction::InputIterator
{
private:
    LInstruction &ins_;
    size_t idx_;
    bool snapshot_;

public:
    InputIterator(LInstruction &ins) :
      ins_(ins),
      idx_(0),
      snapshot_(false)
    { }

    bool more() const {
        if (snapshot_)
            return idx_ < ins_.snapshot()->numEntries();
        if (idx_ < ins_.numOperands())
            return true;
        if (ins_.snapshot() && ins_.snapshot()->numEntries())
            return true;
        return false;
    }

    bool isSnapshotInput() const {
        return snapshot_;
    }

    void next() {
        JS_ASSERT(more());
        idx_++;
        if (!snapshot_ && idx_ == ins_.numOperands() && ins_.snapshot()) {
            idx_ = 0;
            snapshot_ = true;
        }
    }

    void replace(const LAllocation &alloc) {
        if (snapshot_)
            ins_.snapshot()->setEntry(idx_, alloc);
        else
            ins_.setOperand(idx_, alloc);
    }

    LAllocation *operator *() const {
        if (snapshot_)
            return ins_.snapshot()->getEntry(idx_);
        return ins_.getOperand(idx_);
    }

    LAllocation *operator ->() const {
        return **this;
    }
};

class LIRGraph
{
    Vector<LBlock *, 16, SystemAllocPolicy> blocks_;
    js::Vector<Value, 0, SystemAllocPolicy> constantPool_;
    uint32 numVirtualRegisters_;

    // Number of stack slots needed for local spills.
    uint32 localSlotCount_;

    MIRGraph &mir_;

  public:
    LIRGraph(MIRGraph &mir);

    size_t numBlocks() const {
        return blocks_.length();
    }
    LBlock *getBlock(size_t i) const {
        return blocks_[i];
    }
    uint32 numBlockIds() const {
        return mir_.numBlockIds();
    }
    bool addBlock(LBlock *block) {
        return blocks_.append(block);
    }
    uint32 getVirtualRegister() {
        numVirtualRegisters_ += VREG_INCREMENT;
        return numVirtualRegisters_;
    }
    uint32 numVirtualRegisters() const {
        // Virtual registers are 1-based, not 0-based, so add one as a
        // convenience for 0-based arrays.
        return numVirtualRegisters_ + 1;
    } 
    void setLocalSlotCount(uint32 localSlotCount) {
        localSlotCount_ = localSlotCount;
    }
    uint32 localSlotCount() const {
        return localSlotCount_;
    }
    bool addConstantToPool(double d, uint32 *index);
    bool addConstantToPool(MConstant *ins, uint32 *index);
    size_t numConstants() const {
        return constantPool_.length();
    }
    Value *constantPool() {
        return &constantPool_[0];
    }
    const Value &getConstant(size_t index) const {
        return constantPool_[index];
    }
};

LAllocation::LAllocation(const AnyRegister &reg)
{
    if (reg.isFloat())
        *this = LFloatReg(reg.fpu());
    else
        *this = LGeneralReg(reg.gpr());
}

AnyRegister
LAllocation::toRegister() const
{
    JS_ASSERT(isRegister());
    if (isFloatReg())
        return AnyRegister(toFloatReg()->reg());
    return AnyRegister(toGeneralReg()->reg());
}

} // namespace ion
} // namespace js

#define LIR_HEADER(opcode)                                                  \
    Opcode op() const {                                                     \
        return LInstruction::LOp_##opcode;                                  \
    }                                                                       \
    bool accept(LInstructionVisitor *visitor) {                             \
        return visitor->visit##opcode(this);                                \
    }

#include "LIR-Common.h"
#if defined(JS_CPU_X86)
# include "x86/LIR-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/LIR-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/LIR-arm.h"
#endif

#undef LIR_HEADER

#include "IonLIR-inl.h"

#endif // jsion_lir_h__

