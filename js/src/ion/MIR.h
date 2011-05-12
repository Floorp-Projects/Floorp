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

#include "jscntxt.h"
#include "TempAllocPolicy.h"

namespace js {
namespace ion {

#define MIR_OPCODE_LIST(_)                                                  \
    _(Constant)                                                             \
    _(Parameter)                                                            \
    _(Goto)                                                                 \
    _(Test)                                                                 \
    _(Phi)                                                                  \
    _(BitAnd)                                                               \
    _(Return)                                                               \
    _(Copy)

// Representations of IR results.
namespace Representation {
    enum Kind {
        None,           // No representation
        Int32,          // 32-bit integer
        Double,         // 64-bit double
        Box,            // Value box
        Pointer         // GC-thing
    };
}

enum MIRType
{
    MIRType_Double,
    MIRType_Int32,
    MIRType_Undefined,
    MIRType_String,
    MIRType_Boolean,
    MIRType_Magic,
    MIRType_Null,
    MIRType_Object,
    MIRType_Function,
    MIRType_Box
};

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
        return vp.toObject().isFunction()
               ? MIRType_Function
               : MIRType_Object;
      default:
        JS_NOT_REACHED("unexpected jsval type");
        return MIRType_Magic;
    }
}

// Forward declarations of MIR types.
#define FORWARD_DECLARE(op) class M##op;
 MIR_OPCODE_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class MInstruction;
class MBasicBlock;
class MOperand;
class MIRGraph;

class MIRGenerator
{
  public:
    MIRGenerator(JSContext *cx, TempAllocator &temp, JSScript *script, JSFunction *fun,
                 MIRGraph &graph);

    TempAllocator &temp() {
        return temp_;
    }

    JSFunction *fun() const {
        return fun_;
    }
    uint32 nslots() const {
        return nslots_;
    }
    uint32 nargs() const {
        return fun()->nargs;
    }
    uint32 nlocals() const {
        return script->nfixed;
    }
    uint32 calleeSlot() const {
        JS_ASSERT(fun());
        return 0;
    }
    uint32 thisSlot() const {
        JS_ASSERT(fun());
        return 1;
    }
    uint32 firstArgSlot() const {
        JS_ASSERT(fun());
        return 2;
    }
    uint32 argSlot(uint32 i) const {
        return firstArgSlot() + i;
    }
    uint32 firstLocalSlot() const {
        return (fun() ? fun()->nargs + 2 : 0);
    }
    uint32 localSlot(uint32 i) const {
        return firstLocalSlot() + i;
    }
    uint32 firstStackSlot() const {
        return firstLocalSlot() + nlocals();
    }
    uint32 stackSlot(uint32 i) const {
        return firstStackSlot() + i;
    }

    template <typename T>
    T * allocate(size_t count = 1)
    {
        return reinterpret_cast<T *>(temp().allocate(sizeof(T) * count));
    }

  public:
    JSContext *cx;
    JSScript *script;

  protected:
    jsbytecode *pc;
    TempAllocator &temp_;
    JSFunction *fun_;
    uint32 nslots_;
    MIRGraph &graph;
};

// Represents a use of a definition.
class MOperand : public TempObject
{
    friend class MInstruction;

    MOperand *next_;        // Next use in the use chain.
    MInstruction *owner_;   // The instruction that is using this operand.
    uint32 index_;          // The index of this operand in its owner.
    MInstruction *ins_;     // The actual instruction being used.

    MOperand(MInstruction *owner, uint32 index, MInstruction *ins)
      : owner_(owner), index_(index), ins_(ins)
    { }

  private:
    void setIns(MInstruction *ins) {
        ins_ = ins;
    }

  public:
    static inline MOperand *New(MIRGenerator *gen, MInstruction *owner, uint32 index,
                                MInstruction *ins)
    {
        return new (gen->temp()) MOperand(owner, index, ins);
    }

    MInstruction *owner() const {
        return owner_;
    }
    uint32 index() const {
        return index_;
    }
    MInstruction *ins() const {
        return ins_;
    }
    MOperand *next() const {
        return next_;
    }
};

// An MInstruction is an SSA name and definition. It has an opcode, a list of
// operands, and a list of its uses by subsequent instructions.
class MInstruction : public TempObject
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
    // Basic block this instruction lives in.
    MBasicBlock *block_;

    // Dictates how the result of this instruction should be encoded.
    Representation::Kind representation_;

    // The high-level result type of this instruction; must be encodable
    // via the Representation. For example, a double type cannot be
    // represented via an int32, but it can via a Box.
    MIRType type_;

    // Use chain.
    MOperand *uses_;

  private:
    void setBlock(MBasicBlock *block) {
        block_ = block;
    }

  public:
    MInstruction()
      : block_(NULL),
        representation_(Representation::None),
        type_(MIRType_Box),
        uses_(NULL)
    { }

    virtual Opcode op() const = 0;

    Representation::Kind representation() const {
        return representation_;
    }
    void setRepresentation(Representation::Kind representation) {
        representation_ = representation;
    }
    MIRType type() const {
        return type_;
    }
    void setType(MIRType type) {
        type_ = type;
    }

    // The block that contains this instruction.
    MBasicBlock *block() const {
        return block_;
    }

    // Returns this instruction's use chain.
    MOperand *uses() const {
        return uses_;
    }

    // Adds an operand to this instruction's use chain.
    void linkUse(MOperand *operand) {
        operand->next_ = uses_;
        uses_ = operand;
    }

    // Removes an operand from this instruction's use chain.
    void unlinkUse(MOperand *prev, MOperand *use);

    // Returns the instruction at a given operand.
    virtual MInstruction *getOperand(size_t index) const = 0;

    // Replaces an operand in this instruction to use a new instruction.
    // The use is unlinked from its old use chain and relinked into the new
    // one.
    void replaceOperand(MOperand *prev, MOperand *use, MInstruction *ins);

    // Opcode testing and casts.
#   define OPCODE_CASTS(opcode)                                             \
    bool is##opcode() const {                                               \
        return op() == Op_##opcode;                                         \
    }                                                                       \
    inline M##opcode *to##opcode();
    MIR_OPCODE_LIST(OPCODE_CASTS)
#   undef OPCODE_CASTS

  protected:
    // Sets a raw operand; instruction implementation dependent.
    virtual void setOperand(size_t index, MOperand *operand) = 0;

    // Initializes an operand for the first time.
    bool initOperand(MIRGenerator *gen, size_t index, MInstruction *ins) {
        MOperand *operand = MOperand::New(gen, this, index, ins);
        if (!operand)
            return false;
        ins->linkUse(operand);
        setOperand(index, operand);
        return true;
    }
};

template <typename T, size_t Arity>
class MFixedArityList
{
    T list_[Arity];

  public:
    T &operator [](size_t index) {
        JS_ASSERT(index < Arity);
        return list_[index];
    }
    const T &operator [](size_t index) const {
        JS_ASSERT(index < Arity);
        return list_[index];
    }
};

template <typename T>
class MFixedArityList<T, 0>
{
  public:
    T &operator [](size_t index) {
        JS_NOT_REACHED("no items");
        static T *operand = NULL;
        return *operand;
    }
    const T &operator [](size_t index) const {
        JS_NOT_REACHED("no items");
        static T *operand = NULL;
        return *operand;
    }
};

template <size_t Arity>
class MAryInstruction : public MInstruction
{
  protected:
    MFixedArityList<MOperand *, Arity> operands_;

    void setOperand(size_t index, MOperand *operand) {
        operands_[index] = operand;
    }

  public:
    MInstruction *getOperand(size_t index) const {
        return operands_[index]->ins();
    }
};

class MConstant : public MAryInstruction<0>
{
    js::Value value_;

    MConstant(const Value &v);

  public:
    static MConstant *New(MIRGenerator *gen, const Value &v);

    virtual Opcode op() const {
        return MInstruction::Op_Constant;
    }
    const js::Value &value() const {
        return value_;
    }
};

class MParameter : public MAryInstruction<0>
{
    int32 index_;

  public:
    static const int32 CALLEE_SLOT = -2;
    static const int32 THIS_SLOT = -1;

    MParameter(int32 index)
      : index_(index)
    {
        setRepresentation(Representation::Box);
    }

  public:
    static MParameter *New(MIRGenerator *gen, int32 index);

    virtual Opcode op() const {
        return MInstruction::Op_Parameter;
    }

    int32 index() const {
        return index_;
    }
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
};

template <size_t Arity>
class MAryControlInstruction : public MControlInstruction
{
    MFixedArityList<MOperand *, Arity> operands_;

  protected:
    void setOperand(size_t index, MOperand *operand) {
        operands_[index] = operand;
    }

  public:
    MInstruction *getOperand(size_t index) const {
        return operands_[index]->ins();
    }
};

class MGoto : public MAryControlInstruction<0>
{
    MGoto(MBasicBlock *target) {
        successors[0] = target;
    }

  public:
    static MGoto *New(MIRGenerator *gen, MBasicBlock *target);

    virtual Opcode op() const {
        return MInstruction::Op_Goto;
    }
};

class MTest : public MAryControlInstruction<1>
{
    MTest(MBasicBlock *if_true, MBasicBlock *if_false)
    {
        successors[0] = if_true;
        successors[1] = if_false;
    }

  public:
    static MTest *New(MIRGenerator *gen, MInstruction *ins,
                      MBasicBlock *ifTrue, MBasicBlock *ifFalse);

    virtual Opcode op() const {
        return MInstruction::Op_Test;
    }
};

class MReturn : public MAryControlInstruction<1>
{
  public:
    static MReturn *New(MIRGenerator *gen, MInstruction *ins);

    virtual Opcode op() const {
        return MInstruction::Op_Return;
    }
};

class MUnaryInstruction : public MAryInstruction<1>
{
  protected:
    inline bool init(MIRGenerator *gen, MInstruction *ins)
    {
        if (!initOperand(gen, 0, ins))
            return false;
        return true;
    }
};

class MBinaryInstruction : public MAryInstruction<2>
{
  protected:
    inline bool init(MIRGenerator *gen, MInstruction *left, MInstruction *right)
    {
        if (!initOperand(gen, 0, left) ||
            !initOperand(gen, 1, right)) {
            return false;
        }
        return true;
    }
};

class MCopy : public MUnaryInstruction
{
  public:
    static MCopy *New(MIRGenerator  *gen, MInstruction *ins);

    virtual Opcode op() const {
        return MInstruction::Op_Copy;
    }
};

class MBitAnd : public MBinaryInstruction
{
  public:
    static MBitAnd *New(MIRGenerator *gen, MInstruction *left, MInstruction *right);

    virtual Opcode op() const {
        return MInstruction::Op_BitAnd;
    }
};

class MPhi : public MInstruction
{
    js::Vector<MOperand *, 2, TempAllocPolicy> inputs_;

    MPhi(JSContext *cx)
      : inputs_(TempAllocPolicy(cx))
    { }

  protected:
    void setOperand(size_t index, MOperand *operand) {
        inputs_[index] = operand;
    }

  public:
    static MPhi *New(MIRGenerator *gen);

    virtual Opcode op() const {
        return MInstruction::Op_Phi;
    }

    MInstruction *getOperand(size_t index) const {
        return inputs_[index]->ins();
    }
    bool addInput(MIRGenerator *gen, MInstruction *ins);
};

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

