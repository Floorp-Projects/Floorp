/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_LIR_h
#define jit_LIR_h

// This file declares the core data structures for LIR: storage allocations for
// inputs and outputs, as well as the interface instructions must conform to.

#include "mozilla/Array.h"

#include "jit/Bailouts.h"
#include "jit/InlineList.h"
#include "jit/IonAllocPolicy.h"
#include "jit/LOpcodes.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "jit/Registers.h"
#include "jit/Safepoints.h"

namespace js {
namespace jit {

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

static const uint32_t VREG_INCREMENT = 1;

static const uint32_t THIS_FRAME_SLOT = 0;

#if defined(JS_NUNBOX32)
# define BOX_PIECES         2
static const uint32_t VREG_TYPE_OFFSET = 0;
static const uint32_t VREG_DATA_OFFSET = 1;
static const uint32_t TYPE_INDEX = 0;
static const uint32_t PAYLOAD_INDEX = 1;
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

    static const uintptr_t TAG_BIT = 1;
    static const uintptr_t TAG_SHIFT = 0;
    static const uintptr_t TAG_MASK = 1 << TAG_SHIFT;
    static const uintptr_t KIND_BITS = 4;
    static const uintptr_t KIND_SHIFT = TAG_SHIFT + TAG_BIT;
    static const uintptr_t KIND_MASK = (1 << KIND_BITS) - 1;

  protected:
    static const uintptr_t DATA_BITS = (sizeof(uint32_t) * 8) - KIND_BITS - TAG_BIT;
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
        INT_ARGUMENT,   // Argument slot that gets loaded into a GPR.
        DOUBLE_ARGUMENT // Argument slot to be loaded into an FPR
    };

  protected:
    bool isTagged() const {
        return !!(bits_ & TAG_MASK);
    }

    int32_t data() const {
        return int32_t(bits_) >> DATA_SHIFT;
    }
    void setData(int32_t data) {
        JS_ASSERT(int32_t(data) <= int32_t(DATA_MASK));
        bits_ &= ~(DATA_MASK << DATA_SHIFT);
        bits_ |= (data << DATA_SHIFT);
    }
    void setKindAndData(Kind kind, uint32_t data) {
        JS_ASSERT(int32_t(data) <= int32_t(DATA_MASK));
        bits_ = (uint32_t(kind) << KIND_SHIFT) | data << DATA_SHIFT;
    }

    LAllocation(Kind kind, uint32_t data) {
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
        return kind() == INT_ARGUMENT || kind() == DOUBLE_ARGUMENT;
    }
    bool isRegister() const {
        return isGeneralReg() || isFloatReg();
    }
    bool isRegister(bool needFloat) const {
        return needFloat ? isFloatReg() : isGeneralReg();
    }
    bool isMemory() const {
        return isStackSlot() || isArgument();
    }
    bool isDouble() const {
        return kind() == DOUBLE_SLOT || kind() == FPU || kind() == DOUBLE_ARGUMENT;
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

    HashNumber hash() const {
        return bits_;
    }

#ifdef DEBUG
    const char *toString() const;
#else
    const char *toString() const { return "???"; }
#endif

    void dump() const;
};

class LUse : public LAllocation
{
    static const uint32_t POLICY_BITS = 3;
    static const uint32_t POLICY_SHIFT = 0;
    static const uint32_t POLICY_MASK = (1 << POLICY_BITS) - 1;
    static const uint32_t REG_BITS = 5;
    static const uint32_t REG_SHIFT = POLICY_SHIFT + POLICY_BITS;
    static const uint32_t REG_MASK = (1 << REG_BITS) - 1;

    // Whether the physical register for this operand may be reused for a def.
    static const uint32_t USED_AT_START_BITS = 1;
    static const uint32_t USED_AT_START_SHIFT = REG_SHIFT + REG_BITS;
    static const uint32_t USED_AT_START_MASK = (1 << USED_AT_START_BITS) - 1;

  public:
    // Virtual registers get the remaining 20 bits.
    static const uint32_t VREG_BITS = DATA_BITS - (USED_AT_START_SHIFT + USED_AT_START_BITS);
    static const uint32_t VREG_SHIFT = USED_AT_START_SHIFT + USED_AT_START_BITS;
    static const uint32_t VREG_MASK = (1 << VREG_BITS) - 1;

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

        // For snapshot inputs, indicates that the associated instruction will
        // write this input to its output register before bailing out.
        // The register allocator may thus allocate that output register, and
        // does not need to keep the virtual register alive (alternatively,
        // this may be treated as KEEPALIVE).
        RECOVERED_INPUT
    };

    void set(Policy policy, uint32_t reg, bool usedAtStart) {
        setKindAndData(USE, (policy << POLICY_SHIFT) |
                            (reg << REG_SHIFT) |
                            ((usedAtStart ? 1 : 0) << USED_AT_START_SHIFT));
    }

  public:
    LUse(uint32_t vreg, Policy policy, bool usedAtStart = false) {
        set(policy, 0, usedAtStart);
        setVirtualRegister(vreg);
    }
    LUse(Policy policy, bool usedAtStart = false) {
        set(policy, 0, usedAtStart);
    }
    LUse(Register reg, bool usedAtStart = false) {
        set(FIXED, reg.code(), usedAtStart);
    }
    LUse(FloatRegister reg, bool usedAtStart = false) {
        set(FIXED, reg.code(), usedAtStart);
    }
    LUse(Register reg, uint32_t virtualRegister) {
        set(FIXED, reg.code(), false);
        setVirtualRegister(virtualRegister);
    }
    LUse(FloatRegister reg, uint32_t virtualRegister) {
        set(FIXED, reg.code(), false);
        setVirtualRegister(virtualRegister);
    }

    void setVirtualRegister(uint32_t index) {
        JS_ASSERT(index < VREG_MASK);

        uint32_t old = data() & ~(VREG_MASK << VREG_SHIFT);
        setData(old | (index << VREG_SHIFT));
    }

    Policy policy() const {
        Policy policy = (Policy)((data() >> POLICY_SHIFT) & POLICY_MASK);
        return policy;
    }
    uint32_t virtualRegister() const {
        uint32_t index = (data() >> VREG_SHIFT) & VREG_MASK;
        return index;
    }
    uint32_t registerCode() const {
        JS_ASSERT(policy() == FIXED);
        return (data() >> REG_SHIFT) & REG_MASK;
    }
    bool isFixedRegister() const {
        return policy() == FIXED;
    }
    bool usedAtStart() const {
        return !!((data() >> USED_AT_START_SHIFT) & USED_AT_START_MASK);
    }
};

static const uint32_t MAX_VIRTUAL_REGISTERS = LUse::VREG_MASK;

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
    explicit LConstantIndex(uint32_t index)
      : LAllocation(CONSTANT_INDEX, index)
    { }

  public:
    // Used as a placeholder for inputs that can be ignored.
    static LConstantIndex Bogus() {
        return LConstantIndex(0);
    }

    static LConstantIndex FromIndex(uint32_t index) {
        return LConstantIndex(index);
    }

    uint32_t index() const {
        return data();
    }
};

// Stack slots are indexes into the stack, given that each slot is size
// STACK_SLOT_SIZE.
class LStackSlot : public LAllocation
{
  public:
    explicit LStackSlot(uint32_t slot, bool isDouble = false)
      : LAllocation(isDouble ? DOUBLE_SLOT : STACK_SLOT, slot)
    { }

    bool isDouble() const {
        return kind() == DOUBLE_SLOT;
    }

    uint32_t slot() const {
        return data();
    }
};

// Arguments are reverse indexes into the stack, and as opposed to LStackSlot,
// each index is measured in bytes because we have to index the middle of a
// Value on 32 bits architectures.
class LArgument : public LAllocation
{
  public:
    explicit LArgument(LAllocation::Kind kind, int32_t index)
      : LAllocation(kind, index)
    {
        JS_ASSERT(kind == INT_ARGUMENT || kind == DOUBLE_ARGUMENT);
    }

    int32_t index() const {
        return data();
    }
};

// Represents storage for a definition.
class LDefinition
{
    // Bits containing policy, type, and virtual register.
    uint32_t bits_;

    // Before register allocation, this optionally contains a fixed policy.
    // Register allocation assigns this field to a physical policy if none is
    // preset.
    //
    // Right now, pre-allocated outputs are limited to the following:
    //   * Physical argument stack slots.
    //   * Physical registers.
    LAllocation output_;

    static const uint32_t TYPE_BITS = 3;
    static const uint32_t TYPE_SHIFT = 0;
    static const uint32_t TYPE_MASK = (1 << TYPE_BITS) - 1;
    static const uint32_t POLICY_BITS = 2;
    static const uint32_t POLICY_SHIFT = TYPE_SHIFT + TYPE_BITS;
    static const uint32_t POLICY_MASK = (1 << POLICY_BITS) - 1;

    static const uint32_t VREG_BITS = (sizeof(uint32_t) * 8) - (POLICY_BITS + TYPE_BITS);
    static const uint32_t VREG_SHIFT = POLICY_SHIFT + POLICY_BITS;
    static const uint32_t VREG_MASK = (1 << VREG_BITS) - 1;

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
        // the same register. It is not legal to use this if doing so would
        // change the type of the virtual register.
        PASSTHROUGH
    };

    enum Type {
        GENERAL,    // Generic, integer or pointer-width data (GPR).
        OBJECT,     // Pointer that may be collected as garbage (GPR).
        SLOTS,      // Slots/elements pointer that may be moved by minor GCs (GPR).
        DOUBLE,     // 64-bit point value (FPU).
#ifdef JS_NUNBOX32
        // A type virtual register must be followed by a payload virtual
        // register, as both will be tracked as a single gcthing.
        TYPE,
        PAYLOAD
#else
        BOX         // Joined box, for punbox systems. (GPR, gcthing)
#endif
    };

    void set(uint32_t index, Type type, Policy policy) {
        JS_STATIC_ASSERT(MAX_VIRTUAL_REGISTERS <= VREG_MASK);
        bits_ = (index << VREG_SHIFT) | (policy << POLICY_SHIFT) | (type << TYPE_SHIFT);
    }

  public:
    LDefinition(uint32_t index, Type type, Policy policy = DEFAULT) {
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

    LDefinition(uint32_t index, Type type, const LAllocation &a)
      : output_(a)
    {
        set(index, type, PRESET);
    }

    LDefinition() : bits_(0)
    { }

    static LDefinition BogusTemp() {
        return LDefinition(GENERAL, LConstantIndex::Bogus());
    }

    Policy policy() const {
        return (Policy)((bits_ >> POLICY_SHIFT) & POLICY_MASK);
    }
    Type type() const {
        return (Type)((bits_ >> TYPE_SHIFT) & TYPE_MASK);
    }
    uint32_t virtualRegister() const {
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
    void setVirtualRegister(uint32_t index) {
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
    void setReusedInput(uint32_t operand) {
        output_ = LConstantIndex::FromIndex(operand);
    }
    uint32_t getReusedInput() const {
        JS_ASSERT(policy() == LDefinition::MUST_REUSE_INPUT);
        return output_.toConstantIndex()->index();
    }

    static inline Type TypeFrom(MIRType type) {
        switch (type) {
          case MIRType_Boolean:
          case MIRType_Int32:
            return LDefinition::GENERAL;
          case MIRType_String:
          case MIRType_Object:
            return LDefinition::OBJECT;
          case MIRType_Double:
          case MIRType_Float32:
            return LDefinition::DOUBLE;
#if defined(JS_PUNBOX64)
          case MIRType_Value:
            return LDefinition::BOX;
#endif
          case MIRType_Slots:
          case MIRType_Elements:
            return LDefinition::SLOTS;
          case MIRType_Pointer:
            return LDefinition::GENERAL;
          case MIRType_ForkJoinSlice:
            return LDefinition::GENERAL;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected type");
        }
    }
};

// Forward declarations of LIR types.
#define LIROP(op) class L##op;
    LIR_OPCODE_LIST(LIROP)
#undef LIROP

class LSnapshot;
class LSafepoint;
class LInstructionVisitor;

class LInstruction
  : public TempObject,
    public InlineListNode<LInstruction>
{
    uint32_t id_;

    // This snapshot could be set after a ResumePoint.  It is used to restart
    // from the resume point pc.
    LSnapshot *snapshot_;

    // Structure capturing the set of stack slots and registers which are known
    // to hold either gcthings or Values.
    LSafepoint *safepoint_;

  protected:
    MDefinition *mir_;

    LInstruction()
      : id_(0),
        snapshot_(nullptr),
        safepoint_(nullptr),
        mir_(nullptr)
    { }

  public:
    class InputIterator;
    enum Opcode {
#   define LIROP(name) LOp_##name,
        LIR_OPCODE_LIST(LIROP)
#   undef LIROP
        LOp_Invalid
    };

    const char *opName() {
        switch (op()) {
#   define LIR_NAME_INS(name)                   \
            case LOp_##name: return #name;
            LIR_OPCODE_LIST(LIR_NAME_INS)
#   undef LIR_NAME_INS
          default:
            return "Invalid";
        }
    }

    // Hook for opcodes to add extra high level detail about what code will be
    // emitted for the op.
    virtual const char *extraName() const {
        return nullptr;
    }

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

    // Returns the number of successors of this instruction, if it is a control
    // transfer instruction, or zero otherwise.
    virtual size_t numSuccessors() const = 0;
    virtual MBasicBlock *getSuccessor(size_t i) const = 0;
    virtual void setSuccessor(size_t i, MBasicBlock *successor) = 0;

    virtual bool isCall() const {
        return false;
    }
    uint32_t id() const {
        return id_;
    }
    void setId(uint32_t id) {
        JS_ASSERT(!id_);
        JS_ASSERT(id);
        id_ = id;
    }
    LSnapshot *snapshot() const {
        return snapshot_;
    }
    LSafepoint *safepoint() const {
        return safepoint_;
    }
    void setMir(MDefinition *mir) {
        mir_ = mir;
    }
    MDefinition *mirRaw() const {
        /* Untyped MIR for this op. Prefer mir() methods in subclasses. */
        return mir_;
    }
    void assignSnapshot(LSnapshot *snapshot);
    void initSafepoint(TempAllocator &alloc);

    // For an instruction which has a MUST_REUSE_INPUT output, whether that
    // output register will be restored to its original value when bailing out.
    virtual bool recoversInput() const {
        return false;
    }

    virtual void dump(FILE *fp);
    void dump();
    static void printName(FILE *fp, Opcode op);
    virtual void printName(FILE *fp);
    virtual void printOperands(FILE *fp);
    virtual void printInfo(FILE *fp) { }

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
    LInstruction *ins_;

  protected:
    jsbytecode *lastPC_;

    LInstruction *instruction() {
        return ins_;
    }

  public:
    void setInstruction(LInstruction *ins) {
        ins_ = ins;
        if (ins->mirRaw())
            lastPC_ = ins->mirRaw()->trackedPc();
    }

    LInstructionVisitor()
      : ins_(nullptr),
        lastPC_(nullptr)
    {}

  public:
#define VISIT_INS(op) virtual bool visit##op(L##op *) { MOZ_ASSUME_UNREACHABLE("NYI: " #op); }
    LIR_OPCODE_LIST(VISIT_INS)
#undef VISIT_INS
};

typedef InlineList<LInstruction>::iterator LInstructionIterator;
typedef InlineList<LInstruction>::reverse_iterator LInstructionReverseIterator;

class LPhi;
class LMoveGroup;
class LBlock : public TempObject
{
    MBasicBlock *block_;
    Vector<LPhi *, 4, IonAllocPolicy> phis_;
    InlineList<LInstruction> instructions_;
    LMoveGroup *entryMoveGroup_;
    LMoveGroup *exitMoveGroup_;
    Label label_;

    LBlock(TempAllocator &alloc, MBasicBlock *block)
      : block_(block),
        phis_(alloc),
        entryMoveGroup_(nullptr),
        exitMoveGroup_(nullptr)
    { }

  public:
    static LBlock *New(TempAllocator &alloc, MBasicBlock *from) {
        return new(alloc) LBlock(alloc, from);
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
    void clearPhis() {
        phis_.clear();
    }
    MBasicBlock *mir() const {
        return block_;
    }
    LInstructionIterator begin() {
        return instructions_.begin();
    }
    LInstructionIterator begin(LInstruction *at) {
        return instructions_.begin(at);
    }
    LInstructionIterator end() {
        return instructions_.end();
    }
    LInstructionReverseIterator rbegin() {
        return instructions_.rbegin();
    }
    LInstructionReverseIterator rbegin(LInstruction *at) {
        return instructions_.rbegin(at);
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
        JS_ASSERT(!at->isLabel());
        instructions_.insertBefore(at, ins);
    }
    uint32_t firstId();
    uint32_t lastId();
    Label *label() {
        return &label_;
    }
    LMoveGroup *getEntryMoveGroup(TempAllocator &alloc);
    LMoveGroup *getExitMoveGroup(TempAllocator &alloc);
};

template <size_t Defs, size_t Operands, size_t Temps>
class LInstructionHelper : public LInstruction
{
    mozilla::Array<LDefinition, Defs> defs_;
    mozilla::Array<LAllocation, Operands> operands_;
    mozilla::Array<LDefinition, Temps> temps_;

  public:
    size_t numDefs() const MOZ_FINAL MOZ_OVERRIDE {
        return Defs;
    }
    LDefinition *getDef(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        return &defs_[index];
    }
    size_t numOperands() const MOZ_FINAL MOZ_OVERRIDE {
        return Operands;
    }
    LAllocation *getOperand(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        return &operands_[index];
    }
    size_t numTemps() const MOZ_FINAL MOZ_OVERRIDE {
        return Temps;
    }
    LDefinition *getTemp(size_t index) MOZ_FINAL MOZ_OVERRIDE {
        return &temps_[index];
    }

    void setDef(size_t index, const LDefinition &def) MOZ_FINAL MOZ_OVERRIDE {
        defs_[index] = def;
    }
    void setOperand(size_t index, const LAllocation &a) MOZ_FINAL MOZ_OVERRIDE {
        operands_[index] = a;
    }
    void setTemp(size_t index, const LDefinition &a) MOZ_FINAL MOZ_OVERRIDE {
        temps_[index] = a;
    }

    size_t numSuccessors() const {
        return 0;
    }
    MBasicBlock *getSuccessor(size_t i) const {
        JS_ASSERT(false);
        return nullptr;
    }
    void setSuccessor(size_t i, MBasicBlock *successor) {
        JS_ASSERT(false);
    }

    // Default accessors, assuming a single input and output, respectively.
    const LAllocation *input() {
        JS_ASSERT(numOperands() == 1);
        return getOperand(0);
    }
    const LDefinition *output() {
        JS_ASSERT(numDefs() == 1);
        return getDef(0);
    }

    virtual void printInfo(FILE *fp) {
        printOperands(fp);
    }
};

template <size_t Defs, size_t Operands, size_t Temps>
class LCallInstructionHelper : public LInstructionHelper<Defs, Operands, Temps>
{
  public:
    virtual bool isCall() const {
        return true;
    }
};

// An LSnapshot is the reflection of an MResumePoint in LIR. Unlike MResumePoints,
// they cannot be shared, as they are filled in by the register allocator in
// order to capture the precise low-level stack state in between an
// instruction's input and output. During code generation, LSnapshots are
// compressed and saved in the compiled script.
class LSnapshot : public TempObject
{
  private:
    uint32_t numSlots_;
    LAllocation *slots_;
    MResumePoint *mir_;
    SnapshotOffset snapshotOffset_;
    BailoutId bailoutId_;
    BailoutKind bailoutKind_;

    LSnapshot(MResumePoint *mir, BailoutKind kind);
    bool init(MIRGenerator *gen);

  public:
    static LSnapshot *New(MIRGenerator *gen, MResumePoint *snapshot, BailoutKind kind);

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
    BailoutKind bailoutKind() const {
        return bailoutKind_;
    }
    void setBailoutKind(BailoutKind kind) {
        bailoutKind_ = kind;
    }
    void rewriteRecoveredInput(LUse input);
};

struct SafepointNunboxEntry {
    LAllocation type;
    LAllocation payload;

    SafepointNunboxEntry() { }
    SafepointNunboxEntry(LAllocation type, LAllocation payload)
      : type(type), payload(payload)
    { }
};

class LSafepoint : public TempObject
{
    typedef SafepointNunboxEntry NunboxEntry;

  public:
    typedef Vector<uint32_t, 0, IonAllocPolicy> SlotList;
    typedef Vector<NunboxEntry, 0, IonAllocPolicy> NunboxList;

  private:
    // The information in a safepoint describes the registers and gc related
    // values that are live at the start of the associated instruction.

    // The set of registers which are live at an OOL call made within the
    // instruction. This includes any registers for inputs which are not
    // use-at-start, any registers for temps, and any registers live after the
    // call except outputs of the instruction.
    //
    // For call instructions, the live regs are empty. Call instructions may
    // have register inputs or temporaries, which will *not* be in the live
    // registers: if passed to the call, the values passed will be marked via
    // MarkIonExitFrame, and no registers can be live after the instruction
    // except its outputs.
    RegisterSet liveRegs_;

    // The subset of liveRegs which contains gcthing pointers.
    GeneralRegisterSet gcRegs_;

#ifdef CHECK_OSIPOINT_REGISTERS
    // Temp regs of the current instruction. This set is never written to the
    // safepoint; it's only used by assertions during compilation.
    RegisterSet tempRegs_;
#endif

    // Offset to a position in the safepoint stream, or
    // INVALID_SAFEPOINT_OFFSET.
    uint32_t safepointOffset_;

    // Assembler buffer displacement to OSI point's call location.
    uint32_t osiCallPointOffset_;

    // List of stack slots which have gcthing pointers.
    SlotList gcSlots_;

    // List of stack slots which have Values.
    SlotList valueSlots_;

#ifdef JS_NUNBOX32
    // List of registers (in liveRegs) and stack slots which contain pieces of Values.
    NunboxList nunboxParts_;

    // Number of nunboxParts which are not completely filled in.
    uint32_t partialNunboxes_;
#elif JS_PUNBOX64
    // The subset of liveRegs which have Values.
    GeneralRegisterSet valueRegs_;
#endif

    // The subset of liveRegs which contains pointers to slots/elements.
    GeneralRegisterSet slotsOrElementsRegs_;

    // List of stack slots which have slots/elements pointers.
    SlotList slotsOrElementsSlots_;

  public:
    LSafepoint(TempAllocator &alloc)
      : safepointOffset_(INVALID_SAFEPOINT_OFFSET)
      , osiCallPointOffset_(0)
      , gcSlots_(alloc)
      , valueSlots_(alloc)
#ifdef JS_NUNBOX32
      , nunboxParts_(alloc)
      , partialNunboxes_(0)
#endif
      , slotsOrElementsSlots_(alloc)
    { }
    void addLiveRegister(AnyRegister reg) {
        liveRegs_.addUnchecked(reg);
    }
    const RegisterSet &liveRegs() const {
        return liveRegs_;
    }
#ifdef CHECK_OSIPOINT_REGISTERS
    void addTempRegister(AnyRegister reg) {
        tempRegs_.addUnchecked(reg);
    }
    const RegisterSet &tempRegs() const {
        return tempRegs_;
    }
#endif
    void addGcRegister(Register reg) {
        gcRegs_.addUnchecked(reg);
    }
    GeneralRegisterSet gcRegs() const {
        return gcRegs_;
    }
    bool addGcSlot(uint32_t slot) {
        return gcSlots_.append(slot);
    }
    SlotList &gcSlots() {
        return gcSlots_;
    }

    SlotList &slotsOrElementsSlots() {
        return slotsOrElementsSlots_;
    }
    GeneralRegisterSet slotsOrElementsRegs() const {
        return slotsOrElementsRegs_;
    }
    void addSlotsOrElementsRegister(Register reg) {
        slotsOrElementsRegs_.addUnchecked(reg);
    }
    bool addSlotsOrElementsSlot(uint32_t slot) {
        return slotsOrElementsSlots_.append(slot);
    }
    bool addSlotsOrElementsPointer(LAllocation alloc) {
        if (alloc.isStackSlot())
            return addSlotsOrElementsSlot(alloc.toStackSlot()->slot());
        JS_ASSERT(alloc.isRegister());
        addSlotsOrElementsRegister(alloc.toRegister().gpr());
        return true;
    }
    bool hasSlotsOrElementsPointer(LAllocation alloc) {
        if (alloc.isRegister())
            return slotsOrElementsRegs().has(alloc.toRegister().gpr());
        if (alloc.isStackSlot()) {
            for (size_t i = 0; i < slotsOrElementsSlots_.length(); i++) {
                if (slotsOrElementsSlots_[i] == alloc.toStackSlot()->slot())
                    return true;
            }
            return false;
        }
        return false;
    }

    bool addGcPointer(LAllocation alloc) {
        if (alloc.isStackSlot())
            return addGcSlot(alloc.toStackSlot()->slot());
        if (alloc.isRegister())
            addGcRegister(alloc.toRegister().gpr());
        return true;
    }

    bool hasGcPointer(LAllocation alloc) {
        if (alloc.isRegister())
            return gcRegs().has(alloc.toRegister().gpr());
        if (alloc.isStackSlot()) {
            for (size_t i = 0; i < gcSlots_.length(); i++) {
                if (gcSlots_[i] == alloc.toStackSlot()->slot())
                    return true;
            }
            return false;
        }
        JS_ASSERT(alloc.isArgument());
        return true;
    }

    bool addValueSlot(uint32_t slot) {
        return valueSlots_.append(slot);
    }
    SlotList &valueSlots() {
        return valueSlots_;
    }

    bool hasValueSlot(uint32_t slot) {
        for (size_t i = 0; i < valueSlots_.length(); i++) {
            if (valueSlots_[i] == slot)
                return true;
        }
        return false;
    }

#ifdef JS_NUNBOX32

    bool addNunboxParts(LAllocation type, LAllocation payload) {
        return nunboxParts_.append(NunboxEntry(type, payload));
    }

    bool addNunboxType(uint32_t typeVreg, LAllocation type) {
        for (size_t i = 0; i < nunboxParts_.length(); i++) {
            if (nunboxParts_[i].type == type)
                return true;
            if (nunboxParts_[i].type == LUse(typeVreg, LUse::ANY)) {
                nunboxParts_[i].type = type;
                partialNunboxes_--;
                return true;
            }
        }
        partialNunboxes_++;

        // vregs for nunbox pairs are adjacent, with the type coming first.
        uint32_t payloadVreg = typeVreg + 1;
        return nunboxParts_.append(NunboxEntry(type, LUse(payloadVreg, LUse::ANY)));
    }

    bool hasNunboxType(LAllocation type) {
        if (type.isArgument())
            return true;
        if (type.isStackSlot() && hasValueSlot(type.toStackSlot()->slot() + 1))
            return true;
        for (size_t i = 0; i < nunboxParts_.length(); i++) {
            if (nunboxParts_[i].type == type)
                return true;
        }
        return false;
    }

    bool addNunboxPayload(uint32_t payloadVreg, LAllocation payload) {
        for (size_t i = 0; i < nunboxParts_.length(); i++) {
            if (nunboxParts_[i].payload == payload)
                return true;
            if (nunboxParts_[i].payload == LUse(payloadVreg, LUse::ANY)) {
                partialNunboxes_--;
                nunboxParts_[i].payload = payload;
                return true;
            }
        }
        partialNunboxes_++;

        // vregs for nunbox pairs are adjacent, with the type coming first.
        uint32_t typeVreg = payloadVreg - 1;
        return nunboxParts_.append(NunboxEntry(LUse(typeVreg, LUse::ANY), payload));
    }

    bool hasNunboxPayload(LAllocation payload) {
        if (payload.isArgument())
            return true;
        if (payload.isStackSlot() && hasValueSlot(payload.toStackSlot()->slot()))
            return true;
        for (size_t i = 0; i < nunboxParts_.length(); i++) {
            if (nunboxParts_[i].payload == payload)
                return true;
        }
        return false;
    }

    NunboxList &nunboxParts() {
        return nunboxParts_;
    }

    uint32_t partialNunboxes() {
        return partialNunboxes_;
    }

#elif JS_PUNBOX64

    void addValueRegister(Register reg) {
        valueRegs_.add(reg);
    }
    GeneralRegisterSet valueRegs() {
        return valueRegs_;
    }

    bool addBoxedValue(LAllocation alloc) {
        if (alloc.isRegister()) {
            Register reg = alloc.toRegister().gpr();
            if (!valueRegs().has(reg))
                addValueRegister(reg);
            return true;
        }
        if (alloc.isStackSlot()) {
            uint32_t slot = alloc.toStackSlot()->slot();
            for (size_t i = 0; i < valueSlots().length(); i++) {
                if (valueSlots()[i] == slot)
                    return true;
            }
            return addValueSlot(slot);
        }
        JS_ASSERT(alloc.isArgument());
        return true;
    }

    bool hasBoxedValue(LAllocation alloc) {
        if (alloc.isRegister())
            return valueRegs().has(alloc.toRegister().gpr());
        if (alloc.isStackSlot())
            return hasValueSlot(alloc.toStackSlot()->slot());
        JS_ASSERT(alloc.isArgument());
        return true;
    }

#endif // JS_PUNBOX64

    bool encoded() const {
        return safepointOffset_ != INVALID_SAFEPOINT_OFFSET;
    }
    uint32_t offset() const {
        JS_ASSERT(encoded());
        return safepointOffset_;
    }
    void setOffset(uint32_t offset) {
        safepointOffset_ = offset;
    }
    uint32_t osiReturnPointOffset() const {
        // In general, pointer arithmetic on code is bad, but in this case,
        // getting the return address from a call instruction, stepping over pools
        // would be wrong.
        return osiCallPointOffset_ + Assembler::patchWrite_NearCallSize();
    }
    uint32_t osiCallPointOffset() const {
        return osiCallPointOffset_;
    }
    void setOsiCallPointOffset(uint32_t osiCallPointOffset) {
        JS_ASSERT(!osiCallPointOffset_);
        osiCallPointOffset_ = osiCallPointOffset;
    }
    void fixupOffset(MacroAssembler *masm) {
        osiCallPointOffset_ = masm->actualOffset(osiCallPointOffset_);
        safepointOffset_ = masm->actualOffset(safepointOffset_);
    }
};

class LInstruction::InputIterator
{
  private:
    LInstruction &ins_;
    size_t idx_;
    bool snapshot_;

    void handleOperandsEnd() {
        // Iterate on the snapshot when iteration over all operands is done.
        if (!snapshot_ && idx_ == ins_.numOperands() && ins_.snapshot()) {
            idx_ = 0;
            snapshot_ = true;
        }
    }

public:
    InputIterator(LInstruction &ins) :
      ins_(ins),
      idx_(0),
      snapshot_(false)
    {
        handleOperandsEnd();
    }

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
        handleOperandsEnd();
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
    Vector<LBlock *, 16, IonAllocPolicy> blocks_;
    Vector<Value, 0, IonAllocPolicy> constantPool_;
    Vector<LInstruction *, 0, IonAllocPolicy> safepoints_;
    Vector<LInstruction *, 0, IonAllocPolicy> nonCallSafepoints_;
    uint32_t numVirtualRegisters_;
    uint32_t numInstructions_;

    // Number of stack slots needed for local spills.
    uint32_t localSlotCount_;
    // Number of stack slots needed for argument construction for calls.
    uint32_t argumentSlotCount_;

    // Snapshot taken before any LIR has been lowered.
    LSnapshot *entrySnapshot_;

    // LBlock containing LOsrEntry, or nullptr.
    LBlock *osrBlock_;

    MIRGraph &mir_;

  public:
    LIRGraph(MIRGraph *mir);

    MIRGraph &mir() const {
        return mir_;
    }
    size_t numBlocks() const {
        return blocks_.length();
    }
    LBlock *getBlock(size_t i) const {
        return blocks_[i];
    }
    uint32_t numBlockIds() const {
        return mir_.numBlockIds();
    }
    bool addBlock(LBlock *block) {
        return blocks_.append(block);
    }
    uint32_t getVirtualRegister() {
        numVirtualRegisters_ += VREG_INCREMENT;
        return numVirtualRegisters_;
    }
    uint32_t numVirtualRegisters() const {
        // Virtual registers are 1-based, not 0-based, so add one as a
        // convenience for 0-based arrays.
        return numVirtualRegisters_ + 1;
    }
    uint32_t getInstructionId() {
        return numInstructions_++;
    }
    uint32_t numInstructions() const {
        return numInstructions_;
    }
    void setLocalSlotCount(uint32_t localSlotCount) {
        localSlotCount_ = localSlotCount;
    }
    uint32_t localSlotCount() const {
        return AlignBytes(localSlotCount_, StackAlignment / STACK_SLOT_SIZE);
    }
    void setArgumentSlotCount(uint32_t argumentSlotCount) {
        argumentSlotCount_ = argumentSlotCount;
    }
    uint32_t argumentSlotCount() const {
        return argumentSlotCount_;
    }
    uint32_t totalSlotCount() const {
        return localSlotCount() + (argumentSlotCount() * sizeof(Value) / STACK_SLOT_SIZE);
    }
    bool addConstantToPool(const Value &v, uint32_t *index);
    size_t numConstants() const {
        return constantPool_.length();
    }
    Value *constantPool() {
        return &constantPool_[0];
    }
    const Value &getConstant(size_t index) const {
        return constantPool_[index];
    }
    void setEntrySnapshot(LSnapshot *snapshot) {
        JS_ASSERT(!entrySnapshot_);
        JS_ASSERT(snapshot->bailoutKind() == Bailout_Normal);
        snapshot->setBailoutKind(Bailout_ArgumentCheck);
        entrySnapshot_ = snapshot;
    }
    LSnapshot *entrySnapshot() const {
        JS_ASSERT(entrySnapshot_);
        return entrySnapshot_;
    }
    void setOsrBlock(LBlock *block) {
        JS_ASSERT(!osrBlock_);
        osrBlock_ = block;
    }
    LBlock *osrBlock() const {
        return osrBlock_;
    }
    bool noteNeedsSafepoint(LInstruction *ins);
    size_t numNonCallSafepoints() const {
        return nonCallSafepoints_.length();
    }
    LInstruction *getNonCallSafepoint(size_t i) const {
        return nonCallSafepoints_[i];
    }
    size_t numSafepoints() const {
        return safepoints_.length();
    }
    LInstruction *getSafepoint(size_t i) const {
        return safepoints_[i];
    }
    void removeBlock(size_t i);
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

} // namespace jit
} // namespace js

#define LIR_HEADER(opcode)                                                  \
    Opcode op() const {                                                     \
        return LInstruction::LOp_##opcode;                                  \
    }                                                                       \
    bool accept(LInstructionVisitor *visitor) {                             \
        visitor->setInstruction(this);                                      \
        return visitor->visit##opcode(this);                                \
    }

#if defined(JS_NUNBOX32)
# define BOX_OUTPUT_ACCESSORS()                                             \
    const LDefinition *outputType() {                                       \
        return getDef(TYPE_INDEX);                                          \
    }                                                                       \
    const LDefinition *outputPayload() {                                    \
        return getDef(PAYLOAD_INDEX);                                       \
    }
#elif defined(JS_PUNBOX64)
# define BOX_OUTPUT_ACCESSORS()                                             \
    const LDefinition *outputValue() {                                      \
        return getDef(0);                                                   \
    }
#endif

#include "jit/LIR-Common.h"
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
# if defined(JS_CPU_X86)
#  include "jit/x86/LIR-x86.h"
# elif defined(JS_CPU_X64)
#  include "jit/x64/LIR-x64.h"
# endif
# include "jit/shared/LIR-x86-shared.h"
#elif defined(JS_CPU_ARM)
# include "jit/arm/LIR-arm.h"
#endif

#undef LIR_HEADER

namespace js {
namespace jit {

#define LIROP(name)                                                         \
    L##name *LInstruction::to##name()                                       \
    {                                                                       \
        JS_ASSERT(is##name());                                              \
        return static_cast<L##name *>(this);                                \
    }
    LIR_OPCODE_LIST(LIROP)
#undef LIROP

#define LALLOC_CAST(type)                                                   \
    L##type *LAllocation::to##type() {                                      \
        JS_ASSERT(is##type());                                              \
        return static_cast<L##type *>(this);                                \
    }
#define LALLOC_CONST_CAST(type)                                             \
    const L##type *LAllocation::to##type() const {                          \
        JS_ASSERT(is##type());                                              \
        return static_cast<const L##type *>(this);                          \
    }

LALLOC_CAST(Use)
LALLOC_CONST_CAST(Use)
LALLOC_CONST_CAST(GeneralReg)
LALLOC_CONST_CAST(FloatReg)
LALLOC_CONST_CAST(StackSlot)
LALLOC_CONST_CAST(Argument)
LALLOC_CONST_CAST(ConstantIndex)

#undef LALLOC_CAST

#ifdef JS_NUNBOX32
static inline signed
OffsetToOtherHalfOfNunbox(LDefinition::Type type)
{
    JS_ASSERT(type == LDefinition::TYPE || type == LDefinition::PAYLOAD);
    signed offset = (type == LDefinition::TYPE)
                    ? PAYLOAD_INDEX - TYPE_INDEX
                    : TYPE_INDEX - PAYLOAD_INDEX;
    return offset;
}

static inline void
AssertTypesFormANunbox(LDefinition::Type type1, LDefinition::Type type2)
{
    JS_ASSERT((type1 == LDefinition::TYPE && type2 == LDefinition::PAYLOAD) ||
              (type2 == LDefinition::TYPE && type1 == LDefinition::PAYLOAD));
}

static inline unsigned
OffsetOfNunboxSlot(LDefinition::Type type)
{
    if (type == LDefinition::PAYLOAD)
        return NUNBOX32_PAYLOAD_OFFSET / STACK_SLOT_SIZE;
    return NUNBOX32_TYPE_OFFSET / STACK_SLOT_SIZE;
}

// Note that stack indexes for LStackSlot are modelled backwards, so a
// double-sized slot starting at 2 has its next word at 1, *not* 3.
static inline unsigned
BaseOfNunboxSlot(LDefinition::Type type, unsigned slot)
{
    if (type == LDefinition::PAYLOAD)
        return slot + (NUNBOX32_PAYLOAD_OFFSET / STACK_SLOT_SIZE);
    return slot + (NUNBOX32_TYPE_OFFSET / STACK_SLOT_SIZE);
}
#endif

} // namespace jit
} // namespace js

#endif /* jit_LIR_h */
