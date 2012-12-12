/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IonBuilder.h"
#include "LICM.h" // For LinearSum
#include "MIR.h"
#include "MIRGraph.h"
#include "EdgeCaseAnalysis.h"
#include "RangeAnalysis.h"
#include "IonSpewer.h"
#include "jsnum.h"
#include "jsstr.h"
#include "jsatominlines.h"
#include "jstypedarrayinlines.h" // For ClampIntForUint8Array

using namespace js;
using namespace js::ion;

void
MDefinition::PrintOpcodeName(FILE *fp, MDefinition::Opcode op)
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

// If one of the inputs to any non-phi are in a block that will abort, then there is
// no point in processing this instruction, since control flow cannot reach here.
bool
MDefinition::earlyAbortCheck()
{
    if (isPhi())
        return false;
    for (size_t i = 0; i < numOperands(); i++) {
        if (getOperand(i)->block()->earlyAbort()) {
            block()->setEarlyAbort();
            IonSpew(IonSpew_Range, "Ignoring value from block %d because instruction %d is in a block that aborts", block()->id(), getOperand(i)->id());
            return true;
        }
    }
    return false;
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

    Value lhs = left->toConstant()->value();
    Value rhs = right->toConstant()->value();
    Value ret = UndefinedValue();

    switch (ins->op()) {
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
        uint32_t unsignedLhs = (uint32_t)lhs.toInt32();
        ret.setNumber(uint32_t(unsignedLhs >> (rhs.toInt32() & 0x1F)));
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
        ret.setNumber(NumberDiv(lhs.toNumber(), rhs.toNumber()));
        break;
      case MDefinition::Op_Mod:
        ret.setNumber(NumberMod(lhs.toNumber(), rhs.toNumber()));
        break;
      default:
        JS_NOT_REACHED("NYI");
        return NULL;
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
        uint32_t valueNumber = getOperand(i)->valueNumber();
        out = valueNumber + (out << 6) + (out << 16) - out;
    }
    return out;
}

bool
MDefinition::congruentIfOperandsEqual(MDefinition * const &ins) const
{
    if (numOperands() != ins->numOperands())
        return false;

    if (op() != ins->op())
        return false;

    if (type() != ins->type())
        return false;

    if (isEffectful() || ins->isEffectful())
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
MDefinition::analyzeEdgeCasesForward()
{
    return;
}

void
MDefinition::analyzeEdgeCasesBackward()
{
    return;
}
void
MDefinition::analyzeTruncateBackward()
{
    return;
}

MDefinition *
MTest::foldsTo(bool useValueNumbers)
{
    MDefinition *op = getOperand(0);

    if (op->isNot())
        return MTest::New(op->toNot()->operand(), ifFalse(), ifTrue());

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
IsPowerOfTwo(uint32_t n)
{
    return (n > 0) && ((n & (n - 1)) == 0);
}

MConstant *
MConstant::New(const Value &v)
{
    return new MConstant(v);
}

MConstant::MConstant(const js::Value &vp)
  : value_(vp)
{
    setResultType(MIRTypeFromValue(vp));
    setMovable();
}

HashNumber
MConstant::valueHash() const
{
    // This disregards some state, since values are 64 bits. But for a hash,
    // it's completely acceptable.
    return (HashNumber)JSVAL_TO_IMPL(value_).asBits;
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
        fprintf(fp, "0x%x", value().toInt32());
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
      case MIRType_Magic:
        fprintf(fp, "magic");
        break;
      default:
        JS_NOT_REACHED("unexpected type");
        break;
    }
}

void
MConstantElements::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " %p", value());
}

MParameter *
MParameter::New(int32_t index, const types::TypeSet *types)
{
    return new MParameter(index, types);
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
MCall::New(JSFunction *target, size_t maxArgc, size_t numActualArgs, bool construct)
{
    JS_ASSERT(maxArgc >= numActualArgs);
    MCall *ins = new MCall(target, numActualArgs, construct);
    if (!ins->init(maxArgc + NumNonArgumentOperands))
        return NULL;
    return ins;
}

MApplyArgs *
MApplyArgs::New(JSFunction *target, MDefinition *fun, MDefinition *argc, MDefinition *self)
{
    return new MApplyArgs(target, fun, argc, self);
}

MDefinition*
MStringLength::foldsTo(bool useValueNumbers)
{
    if ((type() == MIRType_Int32) && (string()->isConstant())) {
        Value value = string()->toConstant()->value();
        size_t length = JS_GetStringLength(value.toString());

        return MConstant::New(Int32Value(length));
    }

    return this;
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
MTableSwitch::New(MDefinition *ins, int32_t low, int32_t high)
{
    return new MTableSwitch(ins, low, high);
}

MGoto *
MGoto::New(MBasicBlock *target)
{
    JS_ASSERT(target);
    return new MGoto(target);
}

MPhi *
MPhi::New(uint32_t slot)
{
    return new MPhi(slot);
}

MDefinition *
MPhi::foldsTo(bool useValueNumbers)
{
    JS_ASSERT(inputs_.length() != 0);

    MDefinition *first = getOperand(0);

    for (size_t i = 1; i < inputs_.length(); i++) {
        // Phis need dominator information to fold based on value numbers. For
        // simplicity, we only compare SSA names right now (bug 714727).
        if (!EqualValues(false, getOperand(i), first))
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

    return congruentIfOperandsEqual(ins);
}

bool
MPhi::addInput(MDefinition *ins)
{
    ins->addUse(this, inputs_.length());
    return inputs_.append(ins);
}

uint32_t
MPrepareCall::argc() const
{
    JS_ASSERT(useCount() == 1);
    MCall *call = usesBegin()->node()->toDefinition()->toCall();
    return call->numStackArgs();
}

void
MPassArg::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " %d ", argnum_);
    for (size_t j = 0; j < numOperands(); j++) {
        getOperand(j)->printName(fp);
        if (j != numOperands() - 1)
            fprintf(fp, " ");
    }
}

void
MCall::addArg(size_t argnum, MPassArg *arg)
{
    // The operand vector is initialized in reverse order by the IonBuilder.
    // It cannot be checked for consistency until all arguments are added.
    arg->setArgnum(argnum);
    MNode::initOperand(argnum + NumNonArgumentOperands, arg->toDefinition());
}

void
MBitNot::infer(const TypeOracle::UnaryTypes &u)
{
    if (u.inTypes->maybeObject())
        specialization_ = MIRType_None;
    else
        specialization_ = MIRType_Int32;
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

    if (IsConstant(lhs, -1))
        return foldIfNegOne(0);

    if (IsConstant(rhs, -1))
        return foldIfNegOne(1);

    if (EqualValues(useValueNumbers, lhs, rhs))
        return foldIfEqual();

    return this;
}

void
MBinaryBitwiseInstruction::infer(const TypeOracle::BinaryTypes &b)
{
    if (b.lhsTypes->maybeObject() || b.rhsTypes->maybeObject()) {
        specialization_ = MIRType_None;
    } else {
        specialization_ = MIRType_Int32;
        setCommutative();
    }
}

void
MShiftInstruction::infer(const TypeOracle::BinaryTypes &b)
{
    if (b.lhsTypes->maybeObject() || b.rhsTypes->maybeObject())
        specialization_ = MIRType_None;
    else
        specialization_ = MIRType_Int32;
}

void
MUrsh::infer(const TypeOracle::BinaryTypes &b)
{
    if (b.lhsTypes->maybeObject() || b.rhsTypes->maybeObject()) {
        specialization_ = MIRType_None;
        setResultType(MIRType_Value);
        return;
    }

    if (b.outTypes->getKnownTypeTag() == JSVAL_TYPE_DOUBLE) {
        specialization_ = MIRType_Double;
        setResultType(MIRType_Double);
        return;
    }

    specialization_ = MIRType_Int32;
    JS_ASSERT(type() == MIRType_Int32);
}

static inline bool
NeedNegativeZeroCheck(MDefinition *def)
{
    // Test if all uses have the same semantics for -0 and 0
    for (MUseIterator use = def->usesBegin(); use != def->usesEnd(); use++) {
        if (use->node()->isResumePoint())
            continue;

        MDefinition *use_def = use->node()->toDefinition();
        switch (use_def->op()) {
          case MDefinition::Op_Add: {
            // If add is truncating -0 and 0 are observed as the same.
            if (use_def->toAdd()->isTruncated())
                break;

            // x + y gives -0, when both x and y are -0

            // Figure out the order in which the addition's operands will
            // execute. EdgeCaseAnalysis::analyzeLate has renumbered the MIR
            // definitions for us so that this just requires comparing ids.
            MDefinition *first = use_def->getOperand(0);
            MDefinition *second = use_def->getOperand(1);
            if (first->id() > second->id()) {
                MDefinition *temp = first;
                first = second;
                second = temp;
            }

            if (def == first) {
                // Negative zero checks can be removed on the first executed
                // operand only if it is guaranteed the second executed operand
                // will produce a value other than -0. While the second is
                // typed as an int32, a bailout taken between execution of the
                // operands may change that type and cause a -0 to flow to the
                // second.
                //
                // There is no way to test whether there are any bailouts
                // between execution of the operands, so remove negative
                // zero checks from the first only if the second's type is
                // independent from type changes that may occur after bailing.
                switch (second->op()) {
                  case MDefinition::Op_Constant:
                  case MDefinition::Op_BitAnd:
                  case MDefinition::Op_BitOr:
                  case MDefinition::Op_BitXor:
                  case MDefinition::Op_BitNot:
                  case MDefinition::Op_Lsh:
                  case MDefinition::Op_Rsh:
                    break;
                  default:
                    return true;
                }
            }

            // The negative zero check can always be removed on the second
            // executed operand; by the time this executes the first will have
            // been evaluated as int32 and the addition's result cannot be -0.
            break;
          }
          case MDefinition::Op_Sub:
            // If sub is truncating -0 and 0 are observed as the same
            if (use_def->toSub()->isTruncated())
                break;
            /* Fall through...  */
          case MDefinition::Op_StoreElement:
          case MDefinition::Op_StoreElementHole:
          case MDefinition::Op_LoadElement:
          case MDefinition::Op_LoadElementHole:
          case MDefinition::Op_LoadTypedArrayElement:
          case MDefinition::Op_LoadTypedArrayElementHole:
          case MDefinition::Op_CharCodeAt:
          case MDefinition::Op_Mod:
            // Only allowed to remove check when definition is the second operand
            if (use_def->getOperand(0) == def)
                return true;
            if (use_def->numOperands() > 2) {
                for (size_t i = 2; i < use_def->numOperands(); i++) {
                    if (use_def->getOperand(i) == def)
                        return true;
                }
            }
            break;
          case MDefinition::Op_BoundsCheck:
            // Only allowed to remove check when definition is the first operand
            if (use_def->getOperand(1) == def)
                return true;
            break;
          case MDefinition::Op_ToString:
          case MDefinition::Op_FromCharCode:
          case MDefinition::Op_TableSwitch:
          case MDefinition::Op_Compare:
          case MDefinition::Op_BitAnd:
          case MDefinition::Op_BitOr:
          case MDefinition::Op_BitXor:
          case MDefinition::Op_Abs:
          case MDefinition::Op_TruncateToInt32:
            // Always allowed to remove check. No matter which operand.
            break;
          default:
            return true;
        }
    }
    return false;
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

    // 0 + -0 = 0. So we can't remove addition
    if (isAdd() && specialization_ != MIRType_Int32)
        return this;

    if (IsConstant(rhs, getIdentity()))
        return lhs;

    // subtraction isn't commutative. So we can't remove subtraction when lhs equals 0
    if (isSub())
        return this;

    if (IsConstant(lhs, getIdentity()))
        return rhs; // x op id => x

    return this;
}

bool
MAbs::fallible() const
{
    return !range() || !range()->isFinite();
}

MDefinition *
MDiv::foldsTo(bool useValueNumbers)
{
    if (specialization_ == MIRType_None)
        return this;

    if (MDefinition *folded = EvaluateConstantOperands(this))
        return folded;

    return this;
}

void
MDiv::analyzeEdgeCasesForward()
{
    // This is only meaningful when doing integer division.
    if (specialization_ != MIRType_Int32)
        return;

    // Try removing divide by zero check
    if (rhs()->isConstant() && !rhs()->toConstant()->value().isInt32(0))
        canBeDivideByZero_ =  false;

    // If lhs is a constant int != INT32_MIN, then
    // negative overflow check can be skipped.
    if (lhs()->isConstant() && !lhs()->toConstant()->value().isInt32(INT32_MIN))
        setCanBeNegativeZero(false);

    // If rhs is a constant int != -1, likewise.
    if (rhs()->isConstant() && !rhs()->toConstant()->value().isInt32(-1))
        setCanBeNegativeZero(false);

    // If lhs is != 0, then negative zero check can be skipped.
    if (lhs()->isConstant() && !lhs()->toConstant()->value().isInt32(0))
        setCanBeNegativeZero(false);

    // If rhs is >= 0, likewise.
    if (rhs()->isConstant()) {
        const js::Value &val = rhs()->toConstant()->value();
        if (val.isInt32() && val.toInt32() >= 0)
            setCanBeNegativeZero(false);
    }
}

void
MDiv::analyzeEdgeCasesBackward()
{
    if (canBeNegativeZero() && !NeedNegativeZeroCheck(this))
        setCanBeNegativeZero(false);
}

void
MDiv::analyzeTruncateBackward()
{
    if (!isTruncated() && js::ion::EdgeCaseAnalysis::AllUsesTruncate(this))
        setTruncated(true);
}

bool
MDiv::updateForReplacement(MDefinition *ins_)
{
    JS_ASSERT(ins_->isDiv());
    MDiv *ins = ins_->toDiv();
    // Since EdgeCaseAnalysis is not being run before GVN, its information does
    // not need to be merged here.
    setTruncated(isTruncated() && ins->isTruncated());
    return true;
}

static inline MDefinition *
TryFold(MDefinition *original, MDefinition *replacement)
{
    if (original->type() == replacement->type())
        return replacement;
    return original;
}

MDefinition *
MMod::foldsTo(bool useValueNumbers)
{
    if (specialization_ == MIRType_None)
        return this;

    if (MDefinition *folded = EvaluateConstantOperands(this))
        return folded;

    JSRuntime *rt = GetIonContext()->compartment->rt;
    double NaN = rt->NaNValue.toDouble();
    double Inf = rt->positiveInfinityValue.toDouble();

    // Extract double constants.
    bool lhsConstant = lhs()->isConstant() && lhs()->toConstant()->value().isNumber();
    bool rhsConstant = rhs()->isConstant() && rhs()->toConstant()->value().isNumber();

    double lhsd = lhsConstant ? lhs()->toConstant()->value().toNumber() : 0;
    double rhsd = rhsConstant ? rhs()->toConstant()->value().toNumber() : 0;

    // NaN % x -> NaN
    if (lhsConstant && lhsd == NaN)
        return lhs();

    // x % NaN -> NaN
    if (rhsConstant && rhsd == NaN)
        return rhs();

    // x % y -> NaN (where y == 0 || y == -0)
    if (rhsConstant && (rhsd == 0))
        return TryFold(this, MConstant::New(rt->NaNValue));

    // NOTE: y cannot be NaN, 0, or -0 at this point
    // x % y -> x (where x == 0 || x == -0)
    if (lhsConstant && (lhsd == 0))
        return TryFold(this, lhs());

    // x % y -> NaN (where x == Inf || x == -Inf)
    if (lhsConstant && (lhsd == Inf || lhsd == -Inf))
        return TryFold(this, MConstant::New(rt->NaNValue));

    // NOTE: y cannot be NaN, Inf, or -Inf at this point
    // x % y -> x (where y == Inf || y == -Inf)
    if (rhsConstant && (rhsd == Inf || rhsd == -Inf))
        return TryFold(this, lhs());

    return this;
}

void
MAdd::analyzeTruncateBackward()
{
    if (!isTruncated() && js::ion::EdgeCaseAnalysis::AllUsesTruncate(this))
        setTruncated(true);
}

bool
MAdd::updateForReplacement(MDefinition *ins_)
{
    JS_ASSERT(ins_->isAdd());
    MAdd *ins = ins_->toAdd();
    setTruncated(isTruncated() && ins->isTruncated());
    return true;
}

bool
MAdd::fallible()
{
    return !isTruncated() && (!range() || !range()->isFinite());
}

void
MSub::analyzeTruncateBackward()
{
    if (!isTruncated() && js::ion::EdgeCaseAnalysis::AllUsesTruncate(this))
        setTruncated(true);
}

bool
MSub::updateForReplacement(MDefinition *ins_)
{
    JS_ASSERT(ins_->isSub());
    MSub *ins = ins_->toSub();
    setTruncated(isTruncated() && ins->isTruncated());
    return true;
}

bool
MSub::fallible()
{
    return !isTruncated() && (!range() || !range()->isFinite());
}

MDefinition *
MMul::foldsTo(bool useValueNumbers)
{
    MDefinition *out = MBinaryArithInstruction::foldsTo(useValueNumbers);
    if (out != this)
        return out;

    if (specialization() != MIRType_Int32)
        return this;

    if (EqualValues(useValueNumbers, lhs(), rhs()))
        setCanBeNegativeZero(false);

    return this;
}

void
MMul::analyzeEdgeCasesForward()
{
    // Try to remove the check for negative zero
    // This only makes sense when using the integer multiplication
    if (specialization() != MIRType_Int32)
        return;

    // If lhs is > 0, no need for negative zero check.
    if (lhs()->isConstant()) {
        const js::Value &val = lhs()->toConstant()->value();
        if (val.isInt32() && val.toInt32() > 0)
            setCanBeNegativeZero(false);
    }

    // If rhs is > 0, likewise.
    if (rhs()->isConstant()) {
        const js::Value &val = rhs()->toConstant()->value();
        if (val.isInt32() && val.toInt32() > 0)
            setCanBeNegativeZero(false);
    }

}

void
MMul::analyzeEdgeCasesBackward()
{
    if (canBeNegativeZero() && !NeedNegativeZeroCheck(this))
        setCanBeNegativeZero(false);
}

void
MMul::analyzeTruncateBackward()
{
    if (!isPossibleTruncated() && js::ion::EdgeCaseAnalysis::AllUsesTruncate(this))
        setPossibleTruncated(true);
}

bool
MMul::updateForReplacement(MDefinition *ins_)
{
    JS_ASSERT(ins_->isMul());
    MMul *ins = ins_->toMul();
    // setPossibleTruncated can reset the canBenegativeZero check,
    // therefore first query the state, before setting the new state.
    bool truncated = isPossibleTruncated() && ins->isPossibleTruncated();
    bool negativeZero = canBeNegativeZero() || ins->canBeNegativeZero();
    setPossibleTruncated(truncated);
    setCanBeNegativeZero(negativeZero);
    return true;
}

bool
MMul::canOverflow()
{
    if (implicitTruncate_)
        return false;
    return !range() || !range()->isFinite();
}

void
MBinaryArithInstruction::infer(JSContext *cx, const TypeOracle::BinaryTypes &b)
{
    // Retrieve type information of lhs and rhs
    // Rhs is defaulted to int32 first,
    // because in some cases there is no rhs type information
    MIRType lhs = MIRTypeFromValueType(b.lhsTypes->getKnownTypeTag());
    MIRType rhs = MIRType_Int32;

    // Test if types coerces to doubles
    bool lhsCoerces = b.lhsTypes->knownNonStringPrimitive();
    bool rhsCoerces = true;

    // Use type information provided by oracle if available.
    if (b.rhsTypes) {
        rhs = MIRTypeFromValueType(b.rhsTypes->getKnownTypeTag());
        rhsCoerces = b.rhsTypes->knownNonStringPrimitive();
    }

    MIRType rval = MIRTypeFromValueType(b.outTypes->getKnownTypeTag());

    // Don't specialize for neither-integer-nor-double results.
    if (rval != MIRType_Int32 && rval != MIRType_Double) {
        specialization_ = MIRType_None;
        return;
    }

    // Anything complex - strings and objects - are not specialized.
    if (!lhsCoerces || !rhsCoerces) {
        specialization_ = MIRType_None;
        return;
    }

    JS_ASSERT(lhs < MIRType_String || lhs == MIRType_Value);
    JS_ASSERT(rhs < MIRType_String || rhs == MIRType_Value);

    // Don't specialize values when result isn't double
    if (lhs == MIRType_Value || rhs == MIRType_Value) {
        if (rval != MIRType_Double) {
            specialization_ = MIRType_None;
            return;
        }
    }

    // Don't specialize as int32 if one of the operands is undefined,
    // since ToNumber(undefined) is NaN.
    if (rval == MIRType_Int32 && (lhs == MIRType_Undefined || rhs == MIRType_Undefined)) {
        specialization_ = MIRType_None;
        return;
    }

    specialization_ = rval;

    if (isAdd() || isMul())
        setCommutative();
    setResultType(rval);
}

static bool
SafelyCoercesToDouble(JSContext *cx, types::StackTypeSet *types)
{
    types::TypeFlags flags = types->baseFlags();

    // Strings are unhandled -- visitToDouble() doesn't support them yet.
    // Null is unhandled -- ToDouble(null) == 0, but (0 == null) is false.
    types::TypeFlags converts = types::TYPE_FLAG_UNDEFINED | types::TYPE_FLAG_DOUBLE |
                                types::TYPE_FLAG_INT32 | types::TYPE_FLAG_BOOLEAN;

    if ((flags & converts) == flags)
        return true;

    return false;
}

void
MCompare::infer(JSContext *cx, const TypeOracle::BinaryTypes &b)
{
    if (!b.lhsTypes || !b.rhsTypes)
        return;

    MIRType lhs = MIRTypeFromValueType(b.lhsTypes->getKnownTypeTag());
    MIRType rhs = MIRTypeFromValueType(b.rhsTypes->getKnownTypeTag());

    // Strict integer or boolean comparisons may be treated as Int32.
    if ((lhs == MIRType_Int32 && rhs == MIRType_Int32) ||
        (lhs == MIRType_Boolean && rhs == MIRType_Boolean))
    {
        specialization_ = MIRType_Int32;
        return;
    }

    // Loose cross-integer/boolean comparisons may be treated as Int32.
    if (jsop() != JSOP_STRICTEQ && jsop() != JSOP_STRICTNE &&
        (lhs == MIRType_Int32 || lhs == MIRType_Boolean) &&
        (rhs == MIRType_Int32 || rhs == MIRType_Boolean))
    {
        specialization_ = MIRType_Int32;
        return;
    }

    // Numeric comparisons against a double coerce to double.
    if (IsNumberType(lhs) && IsNumberType(rhs)) {
        specialization_ = MIRType_Double;
        return;
    }

    // Handle double comparisons against something that safely coerces to double.
    if (jsop() != JSOP_STRICTEQ && jsop() != JSOP_STRICTNE &&
        ((lhs == MIRType_Double && SafelyCoercesToDouble(cx, b.rhsTypes)) ||
         (rhs == MIRType_Double && SafelyCoercesToDouble(cx, b.lhsTypes))))
    {
        specialization_ = MIRType_Double;
        return;
    }

    if (jsop() == JSOP_STRICTEQ || jsop() == JSOP_EQ ||
        jsop() == JSOP_STRICTNE || jsop() == JSOP_NE)
    {
        if (lhs == MIRType_Object && rhs == MIRType_Object) {
            if (b.lhsTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPECIAL_EQUALITY) ||
                b.rhsTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPECIAL_EQUALITY))
            {
                return;
            }
            specialization_ = MIRType_Object;
            return;
        }

        if (lhs == MIRType_String && rhs == MIRType_String) {
            // We don't yet want to optimize relational string compares.
            specialization_ = MIRType_String;
            return;
        }

        if (IsNullOrUndefined(lhs)) {
            // Lowering expects the rhs to be null/undefined, so we have to
            // swap the operands. This is necessary since we may not know which
            // operand was null/undefined during lowering (both operands may have
            // MIRType_Value).
            specialization_ = lhs;
            swapOperands();
            return;
        }

        if (IsNullOrUndefined(rhs)) {
            specialization_ = rhs;
            return;
        }
    }

    if (jsop() == JSOP_STRICTEQ || jsop() == JSOP_STRICTNE) {
        // bool/bool case got an int32 specialization earlier.
        JS_ASSERT(!(lhs == MIRType_Boolean && rhs == MIRType_Boolean));

        if (lhs == MIRType_Boolean) {
            // Ensure the boolean is on the right so that the type policy knows
            // which side to unbox.
            swapOperands();
            specialization_ = MIRType_Boolean;
            return;
        }
        if (rhs == MIRType_Boolean) {
            specialization_ = MIRType_Boolean;
            return;
        }
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

    if (input->isBitNot() && input->toBitNot()->specialization_ == MIRType_Int32) {
        JS_ASSERT(input->getOperand(0)->type() == MIRType_Int32);
        return input->getOperand(0); // ~~x => x
    }

    return this;
}

MDefinition *
MTypeOf::foldsTo(bool useValueNumbers)
{
    // Note: we can't use input->type() here, type analysis has
    // boxed the input.
    JS_ASSERT(input()->type() == MIRType_Value);

    JSType type;

    switch (inputType()) {
      case MIRType_Double:
      case MIRType_Int32:
        type = JSTYPE_NUMBER;
        break;
      case MIRType_String:
        type = JSTYPE_STRING;
        break;
      case MIRType_Null:
        type = JSTYPE_OBJECT;
        break;
      case MIRType_Undefined:
        type = JSTYPE_VOID;
        break;
      case MIRType_Boolean:
        type = JSTYPE_BOOLEAN;
        break;
      default:
        return this;
    }

    JSRuntime *rt = GetIonContext()->compartment->rt;
    return MConstant::New(StringValue(TypeName(type, rt)));
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
MResumePoint::New(MBasicBlock *block, jsbytecode *pc, MResumePoint *parent, Mode mode)
{
    MResumePoint *resume = new MResumePoint(block, pc, parent, mode);
    if (!resume->init(block))
        return NULL;
    resume->inherit(block);
    return resume;
}

MResumePoint::MResumePoint(MBasicBlock *block, jsbytecode *pc, MResumePoint *caller,
                           Mode mode)
  : MNode(block),
    stackDepth_(block->stackDepth()),
    pc_(pc),
    caller_(caller),
    instruction_(NULL),
    mode_(mode)
{
}

bool
MResumePoint::init(MBasicBlock *block)
{
    operands_ = block->graph().allocate<MDefinition *>(stackDepth());
    if (!operands_)
        return false;
    return true;
}

void
MResumePoint::inherit(MBasicBlock *block)
{
    for (size_t i = 0; i < stackDepth(); i++) {
        MDefinition *def = block->getSlot(i);
        // We have to unwrap MPassArg: it's removed when inlining calls
        // and LStackArg does not define a value.
        if (def->isPassArg())
            def = def->toPassArg()->getArgument();
        initOperand(i, def);
    }
}

MDefinition *
MToInt32::foldsTo(bool useValueNumbers)
{
    MDefinition *input = getOperand(0);
    if (input->type() == MIRType_Int32)
        return input;
    return this;
}

void
MToInt32::analyzeEdgeCasesBackward()
{
    if (!NeedNegativeZeroCheck(this))
        setCanBeNegativeZero(false);
}

MDefinition *
MTruncateToInt32::foldsTo(bool useValueNumbers)
{
    MDefinition *input = getOperand(0);
    if (input->type() == MIRType_Int32)
        return input;

    if (input->type() == MIRType_Double && input->isConstant()) {
        const Value &v = input->toConstant()->value();
        uint32_t ret = ToInt32(v.toDouble());
        return MConstant::New(Int32Value(ret));
    }

    return this;
}

MDefinition *
MToDouble::foldsTo(bool useValueNumbers)
{
    if (input()->isConstant()) {
        const Value &v = input()->toConstant()->value();
        if (v.isNumber()) {
            double out = v.toNumber();
            return MConstant::New(DoubleValue(out));
        }
    }

    return this;
}

MDefinition *
MToString::foldsTo(bool useValueNumbers)
{
    return this;
}

MDefinition *
MClampToUint8::foldsTo(bool useValueNumbers)
{
    if (input()->isConstant()) {
        const Value &v = input()->toConstant()->value();
        if (v.isDouble()) {
            int32_t clamped = ClampDoubleToUint8(v.toDouble());
            return MConstant::New(Int32Value(clamped));
        }
        if (v.isInt32()) {
            int32_t clamped = ClampIntForUint8Array(v.toInt32());
            return MConstant::New(Int32Value(clamped));
        }
    }
    return this;
}

bool
MCompare::tryFold(bool *result)
{
    JSOp op = jsop();

    if (IsNullOrUndefined(specialization())) {
        JS_ASSERT(op == JSOP_EQ || op == JSOP_STRICTEQ ||
                  op == JSOP_NE || op == JSOP_STRICTNE);

        // The LHS is the value we want to test against null or undefined.
        switch (lhs()->type()) {
          case MIRType_Value:
            return false;
          case MIRType_Undefined:
          case MIRType_Null:
            if (lhs()->type() == specialization()) {
                // Both sides have the same type, null or undefined.
                *result = (op == JSOP_EQ || op == JSOP_STRICTEQ);
            } else {
                // One side is null, the other side is undefined. The result is only
                // true for loose equality.
                *result = (op == JSOP_EQ || op == JSOP_STRICTNE);
            }
            return true;
          case MIRType_Int32:
          case MIRType_Double:
          case MIRType_String:
          case MIRType_Object:
          case MIRType_Boolean:
            *result = (op == JSOP_NE || op == JSOP_STRICTNE);
            return true;
          default:
            JS_NOT_REACHED("Unexpected type");
            return false;
        }
    }

    if (specialization_ == MIRType_Boolean) {
        JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);
        JS_ASSERT(rhs()->type() == MIRType_Boolean);

        switch (lhs()->type()) {
          case MIRType_Value:
            return false;
          case MIRType_Int32:
          case MIRType_Double:
          case MIRType_String:
          case MIRType_Object:
          case MIRType_Null:
          case MIRType_Undefined:
            *result = (op == JSOP_STRICTNE);
            return true;
          case MIRType_Boolean:
            // Int32 specialization should handle this.
            JS_NOT_REACHED("Wrong specialization");
            return false;
          default:
            JS_NOT_REACHED("Unexpected type");
            return false;
        }
    }

    return false;
}

bool
MCompare::evaluateConstantOperands(bool *result)
{
    if (type() != MIRType_Boolean)
        return false;

    MDefinition *left = getOperand(0);
    MDefinition *right = getOperand(1);

    if (!left->isConstant() || !right->isConstant())
        return false;

    Value lhs = left->toConstant()->value();
    Value rhs = right->toConstant()->value();

    // Fold away some String equality comparisons.
    if (lhs.isString() && rhs.isString()) {
        int32_t comp = 0; // Default to equal.
        if (left != right) {
            if (!CompareStrings(GetIonContext()->cx, lhs.toString(), rhs.toString(), &comp))
                return false;
        }
        
        switch (jsop_) {
          case JSOP_LT:
            *result = (comp < 0);
            break;
          case JSOP_LE:
            *result = (comp <= 0);
            break;
          case JSOP_GT:
            *result = (comp > 0);
            break;
          case JSOP_GE:
            *result = (comp >= 0);
            break;
          case JSOP_STRICTEQ: // Fall through.
          case JSOP_EQ:
            *result = (comp == 0);
            break;
          case JSOP_STRICTNE: // Fall through.
          case JSOP_NE:
            *result = (comp != 0);
            break;
          default:
            JS_NOT_REACHED("Unexpected op.");
            return false;
        }

        return true;
    }

    if (!lhs.isNumber() || !rhs.isNumber())
        return false;

    switch (jsop_) {
      case JSOP_LT:
        *result = (lhs.toNumber() < rhs.toNumber());
        break;
      case JSOP_LE:
        *result = (lhs.toNumber() <= rhs.toNumber());
        break;
      case JSOP_GT:
        *result = (lhs.toNumber() > rhs.toNumber());
        break;
      case JSOP_GE:
        *result = (lhs.toNumber() >= rhs.toNumber());
        break;
      case JSOP_EQ:
        *result = (lhs.toNumber() == rhs.toNumber());
        break;
      case JSOP_NE:
        *result = (lhs.toNumber() != rhs.toNumber());
        break;
      default:
        return false;
    }

    return true;
}

MDefinition *
MCompare::foldsTo(bool useValueNumbers)
{
    bool result;

    if (tryFold(&result))
        return MConstant::New(BooleanValue(result));
    else if (evaluateConstantOperands(&result))
        return MConstant::New(BooleanValue(result));

    return this;
}

MDefinition *
MNot::foldsTo(bool useValueNumbers)
{
    // Fold if the input is constant
    if (operand()->isConstant()) {
       const Value &v = operand()->toConstant()->value();
        // ValueToBoolean can cause no side-effects, so this is safe.
        return MConstant::New(BooleanValue(!ToBoolean(v)));
    }

    // NOT of an object is always false
    if (operand()->type() == MIRType_Object)
        return MConstant::New(BooleanValue(false));

    // NOT of an undefined or null value is always true
    if (operand()->type() == MIRType_Undefined || operand()->type() == MIRType_Null)
        return MConstant::New(BooleanValue(true));

    return this;
}

bool
MBoundsCheckLower::fallible()
{
    return !range() || range()->lower() < minimum_;
}

void
MBeta::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " ");
    getOperand(0)->printName(fp);
    fprintf(fp, " ");

    Sprinter sp(GetIonContext()->cx);
    sp.init();
    comparison_->print(sp);
    fprintf(fp, "%s", sp.string());
}

void
MBeta::computeRange()
{
    bool emptyRange = false;

    Range *range = Range::intersect(val_->range(), comparison_, &emptyRange);
    if (emptyRange) {
        IonSpew(IonSpew_Range, "Marking block for inst %d unexitable", id());
        block()->setEarlyAbort();
    } else {
        setRange(range);
    }
}
