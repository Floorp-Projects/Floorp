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

#include "IonBuilder.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace js::ion;

static void
PrintOpcodeName(FILE *fp, MDefinition::Opcode op)
{
    static const char *names[] =
    {
#define NAME(x) #x,
        MIR_OPCODE_LIST(NAME)
#undef NAME
    };
    const char *name = names[op];
    size_t len = strlen(name);
    for (size_t i = 0; i < len; i++)
        fprintf(fp, "%c", tolower(name[i]));
}

static inline bool
EqualValues(bool useGVN, MDefinition *left, MDefinition *right)
{
    if (useGVN)
        return left->valueNumber() == right->valueNumber();

    return left->id() == right->id();
}

static MConstant *
EvaluateConstantOperands(MBinaryInstruction *ins)
{
    MDefinition *left = ins->getOperand(0);
    MDefinition *right = ins->getOperand(1);

    if (!left->isConstant() || !right->isConstant())
        return NULL;

    js::Value lhs = left->toConstant()->value();
    js::Value rhs = right->toConstant()->value();
    js::Value ret;

    switch(ins->op()) {
      case MDefinition::Op_BitAnd:
        ret = Int32Value(lhs.toInt32() & rhs.toInt32());
        break;
      case MDefinition::Op_BitOr:
        ret = Int32Value(lhs.toInt32() | rhs.toInt32());
        break;
      case MDefinition::Op_BitXor:
        ret = Int32Value(lhs.toInt32() ^ rhs.toInt32());
        break;
      case MDefinition::Op_Lsh:
        ret = Int32Value(lhs.toInt32() << (rhs.toInt32() & 0x1F));
        break;
      case MDefinition::Op_Rsh:
        ret = Int32Value(lhs.toInt32() >> (rhs.toInt32() & 0x1F));
        break;
      case MDefinition::Op_Ursh: {
        if (lhs.toInt32() < 0 && rhs.toInt32() == 0)
            return NULL;
        uint32 unsignedLhs = (uint32_t)lhs.toInt32();
        ret = Int32Value(uint32(unsignedLhs >> (rhs.toInt32() & 0x1F)));
        break;
      }
      case MDefinition::Op_Add:
        ret.setNumber(lhs.toNumber() + rhs.toNumber());
        break;
      case MDefinition::Op_Sub:
        ret.setNumber(lhs.toNumber() - rhs.toNumber());
        break;
      case MDefinition::Op_Mul:
        ret.setNumber(lhs.toNumber() * rhs.toNumber());
        break;
      case MDefinition::Op_Div:
        ret.setNumber(lhs.toNumber() / rhs.toNumber());
        break;
      default:
        JS_NOT_REACHED("NYI");
    }

    if (ins->type() != MIRTypeFromValue(ret))
        return NULL;

    return MConstant::New(ret);
}

void
MDefinition::printName(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, "%u", id());

    if (valueNumber() != 0)
        fprintf(fp, "-vn%u", valueNumber());
}

HashNumber
MDefinition::valueHash() const
{
    HashNumber out = op();
    for (size_t i = 0; i < numOperands(); i++) {
        uint32 valueNumber = getOperand(i)->valueNumber();
        out = valueNumber + (out << 6) + (out << 16) - out;
    }
    return out;
}

bool
MDefinition::congruentTo(MDefinition * const &ins) const
{
    if (numOperands() != ins->numOperands())
        return false;

    if (op() != ins->op())
        return false;

    if (type() != ins->type())
        return false;

    for (size_t i = 0; i < numOperands(); i++) {
        if (getOperand(i)->valueNumber() != ins->getOperand(i)->valueNumber())
            return false;
    }

    return true;
}

MDefinition *
MDefinition::foldsTo(bool useValueNumbers)
{
    // In the default case, there are no constants to fold.
    return this;
}

void
MDefinition::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " ");
    for (size_t j = 0; j < numOperands(); j++) {
        getOperand(j)->printName(fp);
        if (j != numOperands() - 1)
            fprintf(fp, " ");
    }
}

size_t
MDefinition::useCount() const
{
    size_t count = 0;
    for (MUseIterator i(uses_.begin()); i != uses_.end(); i++)
        count++;
    return count;
}

MUseIterator
MDefinition::removeUse(MUseIterator use)
{
    return uses_.removeAt(use);
}

MUseIterator
MNode::replaceOperand(MUseIterator use, MDefinition *ins)
{
    MDefinition *used = getOperand(use->index());
    if (used == ins)
        return use;

    MUse *save = *use;
    MUseIterator result(used->removeUse(use));
    if (ins) {
        setOperand(save->index(), ins);
        ins->linkUse(save);
    }
    return result;
}

void
MNode::replaceOperand(size_t index, MDefinition *def)
{
    MDefinition *d = getOperand(index);
    for (MUseIterator i(d->usesBegin()); i != d->usesEnd(); i++) {
        if (i->index() == index && i->node() == this) {
            replaceOperand(i, def);
            return;
        }
    }

    JS_NOT_REACHED("could not find use");
}

void
MDefinition::replaceAllUsesWith(MDefinition *dom)
{
    for (MUseIterator i(uses_.begin()); i != uses_.end(); ) {
        MUse *use = *i;
        i = uses_.removeAt(i);
        use->node()->setOperand(use->index(), dom);
        dom->linkUse(use);
    }
}

static inline bool
IsPowerOfTwo(uint32 n)
{
    return (n > 0) && ((n & (n - 1)) == 0);
}

MIRType
MDefinition::usedAsType() const
{
    // usedTypes() should never have MIRType_Value in its set.
    JS_ASSERT(!(usedTypes() & (1 << MIRType_Value)));

    if (IsPowerOfTwo(usedTypes())) {
        // If all uses of this instruction want a specific type, then set the
        // result as that type.
        int t;
        JS_FLOOR_LOG2(t, usedTypes());
        return MIRType(t);
    }

    return MIRType_Value;
}

MConstant *
MConstant::New(const Value &v)
{
    return new MConstant(v);
}

MConstant::MConstant(const js::Value &vp)
  : value_(vp),
    constantPoolIndex_(0)
{
    setResultType(MIRTypeFromValue(vp));
    setIdempotent();
}


HashNumber
MConstant::valueHash() const
{
    // This disregards some state, since values are 64 bits. But for a hash,
    // it's completely acceptable.
    return (HashNumber)value_.asRawBits();
}
bool
MConstant::congruentTo(MDefinition * const &ins) const
{
    if (!ins->isConstant())
        return false;
    return ins->toConstant()->value() == value();
}

void
MConstant::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " ");
    switch (type()) {
      case MIRType_Undefined:
        fprintf(fp, "undefined");
        break;
      case MIRType_Null:
        fprintf(fp, "null");
        break;
      case MIRType_Boolean:
        fprintf(fp, value().toBoolean() ? "true" : "false");
        break;
      case MIRType_Int32:
        fprintf(fp, "%x", value().toInt32());
        break;
      case MIRType_Double:
        fprintf(fp, "%f", value().toDouble());
        break;
      case MIRType_Object:
        fprintf(fp, "object %p (%s)", (void *)&value().toObject(),
                value().toObject().getClass()->name);
        break;
      case MIRType_String:
        fprintf(fp, "string %p", (void *)value().toString());
        break;
      default:
        JS_NOT_REACHED("unexpected type");
        break;
    }
}

MParameter *
MParameter::New(int32 index, types::TypeSet *types)
{
    MParameter *res = new MParameter(index);
    if (types)
        res->setTypeSet(types);
    return res;
}

void
MParameter::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " %d", index());
}

HashNumber
MParameter::valueHash() const
{
    return index_; // Why not?
}

bool
MParameter::congruentTo(MDefinition * const &ins) const
{
    if (!ins->isParameter())
        return false;

    return ins->toParameter()->index() == index_;
}

MCall *
MCall::New(size_t argc)
{
    MCall *ins = new MCall;
    if (!ins->init(argc + NumNonArgumentOperands))
        return NULL;
    return ins;
}

MCopy *
MCopy::New(MDefinition *ins)
{
    // Don't create nested copies.
    if (ins->isCopy())
        ins = ins->toCopy()->getOperand(0);

    return new MCopy(ins);
}

HashNumber
MCopy::valueHash() const
{
    return getOperand(0)->valueHash();
}

bool
MCopy::congruentTo(MDefinition * const &ins) const
{
    if (!ins->isCopy())
        return false;

    return ins->toCopy()->getOperand(0) == getOperand(0);
}

MTest *
MTest::New(MDefinition *ins, MBasicBlock *ifTrue, MBasicBlock *ifFalse)
{
    return new MTest(ins, ifTrue, ifFalse);
}

MCompare *
MCompare::New(MDefinition *left, MDefinition *right, JSOp op)
{
    return new MCompare(left, right, op);
}

MTableSwitch *
MTableSwitch::New(MDefinition *ins, int32 low, int32 high)
{
    return new MTableSwitch(ins, low, high);
}

MGoto *
MGoto::New(MBasicBlock *target)
{
    return new MGoto(target);
}

MPhi *
MPhi::New(uint32 slot)
{
    return new MPhi(slot);
}

MDefinition *
MPhi::foldsTo(bool useValueNumbers)
{
    JS_ASSERT(inputs_.length() != 0);

    MDefinition *first = getOperand(0);

    for (size_t i = 1; i < inputs_.length(); i++) {
        if (!EqualValues(useValueNumbers, getOperand(i), first))
            return this;
    }

    return first;
}


bool
MPhi::congruentTo(MDefinition *const &ins) const
{
    if (!ins->isPhi())
        return false;
    // Since we do not know which predecessor we are merging from, we must
    // assume that phi instructions in different blocks are not equal.
    // (Bug 674656)
    if (ins->block()->id() != block()->id())
        return false;

    return MDefinition::congruentTo(ins);
}

bool
MPhi::addInput(MDefinition *ins)
{
    ins->addUse(this, inputs_.length());
    return inputs_.append(ins);
}

MReturn *
MReturn::New(MDefinition *ins)
{
    return new MReturn(ins);
}

uint32
MPrepareCall::argc() const
{
    JS_ASSERT(useCount() == 1);
    MCall *call = usesBegin()->node()->toDefinition()->toCall();
    return call->argc();
}

void
MCall::addArg(size_t argnum, MPassArg *arg)
{
    // The operand vector is initialized in reverse order by the IonBuilder.
    // It cannot be checked for consistency until all arguments are added.
    arg->setArgnum(argnum);
    return MNode::initOperand(argnum + NumNonArgumentOperands, arg->toDefinition());
}

void
MBitNot::infer(const TypeOracle::Unary &u)
{
    if (u.ival == MIRType_Object) {
        specialization_ = MIRType_None;
    } else {
        specialization_ = MIRType_Int32;
        setIdempotent();
    }
}

static inline bool
IsConstant(MDefinition *def, double v)
{
    return def->isConstant() && def->toConstant()->value().toNumber() == v;
}

MDefinition *
MBinaryBitwiseInstruction::foldsTo(bool useValueNumbers)
{
    if (specialization_ != MIRType_Int32)
        return this;

    MDefinition *lhs = getOperand(0);
    MDefinition *rhs = getOperand(1);

    if (MDefinition *folded = EvaluateConstantOperands(this))
        return folded;

    if (IsConstant(lhs, 0))
        return foldIfZero(0);

    if (IsConstant(rhs, 0))
        return foldIfZero(1);

    if (EqualValues(useValueNumbers, lhs, rhs))
        return foldIfEqual();

    return this;
}

void
MBinaryBitwiseInstruction::infer(const TypeOracle::Binary &b)
{
    if (b.lhs == MIRType_Object || b.rhs == MIRType_Object) {
        specialization_ = MIRType_None;
    } else {
        specialization_ = MIRType_Int32;
        setIdempotent();
        setCommutative();
    }
}

MDefinition *
MBinaryArithInstruction::foldsTo(bool useValueNumbers)
{
    if (specialization_ == MIRType_None)
        return this;

    MDefinition *lhs = getOperand(0);
    MDefinition *rhs = getOperand(1);
    if (MDefinition *folded = EvaluateConstantOperands(this))
        return folded;

    if (IsConstant(lhs, getIdentity()))
        return rhs; // x op id => x

    if (IsConstant(rhs, getIdentity()))
        return lhs;

    return this;
}

MDefinition *
MDiv::foldsTo(bool useValueNumbers)
{
    if (specialization_ == MIRType_None)
        return this;

    MDefinition *lhs = getOperand(0);
    MDefinition *rhs = getOperand(1);
    if (MDefinition *folded = EvaluateConstantOperands(this))
        return folded;

    // 0 / x -> 0
    // x / 1 -> x
    if (IsConstant(lhs, 0) || IsConstant(rhs, 1))
        return lhs;

    return this;
}

void
MBinaryArithInstruction::infer(const TypeOracle::Binary &b)
{
    if (b.lhs == MIRType_Int32 && b.rhs == MIRType_Int32) {
        specialization_ = MIRType_Int32;
        setIdempotent();
        setCommutative();
        setResultType(specialization_);
    } else if (b.lhs == MIRType_Double && b.rhs == MIRType_Double) {
        specialization_ = MIRType_Double;
        setIdempotent();
        setCommutative();
        setResultType(specialization_);
    } else if (b.lhs < MIRType_String && b.rhs < MIRType_String) {
        specialization_ = MIRType_Any;
        if (CoercesToDouble(b.lhs) || CoercesToDouble(b.rhs))
            setResultType(MIRType_Double);
        else
            setResultType(MIRType_Int32);
    } else {
        specialization_ = MIRType_None;
    }
}

void
MCompare::infer(const TypeOracle::Binary &b)
{
    // If neither operand is an object, then we are idempotent
    if (b.lhs != MIRType_Object && b.rhs != MIRType_Object)
        setIdempotent();

    // Set specialization
    if (b.lhs < MIRType_String && b.rhs < MIRType_String) {
        if (CoercesToDouble(b.lhs) || CoercesToDouble(b.rhs))
            specialization_ = MIRType_Double;
        else
            specialization_ = MIRType_Int32;
    }
    else {
        specialization_ = MIRType_None;
    }
}

MBitNot *
MBitNot::New(MDefinition *input)
{
    return new MBitNot(input);
}

MDefinition *
MBitNot::foldsTo(bool useValueNumbers)
{
    if (specialization_ != MIRType_Int32)
        return this;

    MDefinition *input = getOperand(0);

    if (input->isConstant()) {
        js::Value v = Int32Value(~(input->toConstant()->value().toInt32()));
        return MConstant::New(v);
    }

    if (input->isBitNot())
        return input->getOperand(0); // ~~x => x

    return this;
}

MBitAnd *
MBitAnd::New(MDefinition *left, MDefinition *right)
{
    return new MBitAnd(left, right);
}

MBitOr *
MBitOr::New(MDefinition *left, MDefinition *right)
{
    return new MBitOr(left, right);
}

MBitXor *
MBitXor::New(MDefinition *left, MDefinition *right)
{
    return new MBitXor(left, right);
}

MLsh *
MLsh::New(MDefinition *left, MDefinition *right)
{
    return new MLsh(left, right);
}

MRsh *
MRsh::New(MDefinition *left, MDefinition *right)
{
    return new MRsh(left, right);
}

MUrsh *
MUrsh::New(MDefinition *left, MDefinition *right)
{
    return new MUrsh(left, right);
}

MResumePoint *
MResumePoint::New(MBasicBlock *block, jsbytecode *pc)
{
    MResumePoint *resume = new MResumePoint(block, pc);
    if (!resume->init(block))
        return NULL;
    resume->inherit(block);
    return resume;
}

MResumePoint::MResumePoint(MBasicBlock *block, jsbytecode *pc)
  : MNode(block),
    stackDepth_(block->stackDepth()),
    pc_(pc)
{
}

bool
MResumePoint::init(MBasicBlock *block)
{
    operands_ = block->gen()->allocate<MDefinition *>(stackDepth());
    if (!operands_)
        return false;
    return true;
}

void
MResumePoint::inherit(MBasicBlock *block)
{
    for (size_t i = 0; i < stackDepth(); i++)
        initOperand(i, block->getSlot(i));
}

