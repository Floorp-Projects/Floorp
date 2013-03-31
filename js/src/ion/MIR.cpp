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
}

void
MDefinition::analyzeEdgeCasesBackward()
{
}

static bool
MaybeEmulatesUndefined(types::StackTypeSet *types, JSContext *cx)
{
    if (!types->maybeObject())
        return false;
    return types->hasObjectFlags(cx, types::OBJECT_FLAG_EMULATES_UNDEFINED);
}

void
MTest::infer(const TypeOracle::UnaryTypes &u, JSContext *cx)
{
    if (!u.inTypes)
        return;

    JS_ASSERT(operandMightEmulateUndefined());

    if (!MaybeEmulatesUndefined(u.inTypes, cx))
        markOperandCantEmulateUndefined();
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
MNode::replaceOperand(MUseIterator use, MDefinition *def)
{
    JS_ASSERT(def != NULL);
    uint32_t index = use->index();
    MDefinition *prev = use->producer();

    JS_ASSERT(use->index() < numOperands());
    JS_ASSERT(use->producer() == getOperand(index));
    JS_ASSERT(use->consumer() == this);

    if (prev == def)
        return use;

    MUseIterator result(prev->removeUse(use));
    setOperand(index, def);
    return result;
}

void
MNode::replaceOperand(size_t index, MDefinition *def)
{
    JS_ASSERT(def != NULL);
    MUse *use = getUseFor(index);
    MDefinition *prev = use->producer();

    JS_ASSERT(use->index() == index);
    JS_ASSERT(use->index() < numOperands());
    JS_ASSERT(use->producer() == getOperand(index));
    JS_ASSERT(use->consumer() == this);

    if (prev == def)
        return;

    prev->removeUse(use);
    setOperand(index, def);
}

void
MNode::discardOperand(size_t index)
{
    MUse *use = getUseFor(index);

    JS_ASSERT(use->index() == index);
    JS_ASSERT(use->producer() == getOperand(index));
    JS_ASSERT(use->consumer() == this);

    use->producer()->removeUse(use);

#ifdef DEBUG
    // Causes any producer/consumer lookups to trip asserts.
    use->set(NULL, NULL, index);
#endif
}

void
MDefinition::replaceAllUsesWith(MDefinition *dom)
{
    JS_ASSERT(dom != NULL);
    if (dom == this)
        return;

    for (MUseIterator i(usesBegin()); i != usesEnd(); ) {
        JS_ASSERT(i->producer() == this);
        i = i->consumer()->replaceOperand(i, dom);
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
        if (value().toObject().isFunction()) {
            JSFunction *fun = value().toObject().toFunction();
            if (fun->displayAtom()) {
                fputs("function ", fp);
                FileEscapedString(fp, fun->displayAtom(), 0);
            } else {
                fputs("unnamed function", fp);
            }
            if (fun->hasScript()) {
                RawScript script = fun->nonLazyScript();
                fprintf(fp, " (%s:%u)",
                        script->filename() ? script->filename() : "", script->lineno);
            }
            fprintf(fp, " at %p", (void *) fun);
            break;
        }
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
MParameter::New(int32_t index, const types::StackTypeSet *types)
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
MCall::New(JSFunction *target, size_t maxArgc, size_t numActualArgs, bool construct,
           types::StackTypeSet *calleeTypes)
{
    JS_ASSERT(maxArgc >= numActualArgs);
    MCall *ins = new MCall(target, numActualArgs, construct, calleeTypes);
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

MCompare *
MCompare::NewAsmJS(MDefinition *left, MDefinition *right, JSOp op, CompareType compareType)
{
    JS_ASSERT(compareType == Compare_Int32 || compareType == Compare_UInt32 ||
              compareType == Compare_Double);
    MCompare *comp = new MCompare(left, right, op);
    comp->compareType_ = compareType;
    comp->operandMightEmulateUndefined_ = false;
    comp->setResultType(MIRType_Int32);
    return comp;
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

void
MUnbox::printOpcode(FILE *fp)
{
    PrintOpcodeName(fp, op());
    fprintf(fp, " ");
    getOperand(0)->printName(fp);
    fprintf(fp, " ");

    switch (type()) {
      case MIRType_Int32: fprintf(fp, "to Int32"); break;
      case MIRType_Double: fprintf(fp, "to Double"); break;
      case MIRType_Boolean: fprintf(fp, "to Boolean"); break;
      case MIRType_String: fprintf(fp, "to String"); break;
      case MIRType_Object: fprintf(fp, "to Object"); break;
      default: break;
    }

    switch (mode()) {
      case Fallible: fprintf(fp, " (fallible)"); break;
      case Infallible: fprintf(fp, " (infallible)"); break;
      case TypeBarrier: fprintf(fp, " (typebarrier)"); break;
      case TypeGuard: fprintf(fp, " (typeguard)"); break;
      default: break;
    }
}

MPhi *
MPhi::New(uint32_t slot)
{
    return new MPhi(slot);
}

void
MPhi::removeOperand(size_t index)
{
    MUse *use = getUseFor(index);

    JS_ASSERT(index < inputs_.length());
    JS_ASSERT(inputs_.length() > 1);

    JS_ASSERT(use->index() == index);
    JS_ASSERT(use->producer() == getOperand(index));
    JS_ASSERT(use->consumer() == this);

    // Remove use from producer's use chain.
    use->producer()->removeUse(use);

    // If we have phi(..., a, b, c, d, ..., z) and we plan
    // on removing a, then first shift downward so that we have
    // phi(..., b, c, d, ..., z, z):
    size_t length = inputs_.length();
    for (size_t i = index; i < length - 1; i++) {
        MUse *next = MPhi::getUseFor(i + 1);
        next->producer()->removeUse(next);
        MPhi::setOperand(i, next->producer());
    }

    // truncate the inputs_ list:
    inputs_.shrinkBy(1);
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
MPhi::reserveLength(size_t length)
{
    // Initializes a new MPhi to have an Operand vector of at least the given
    // capacity. This permits use of addInput() instead of addInputSlow(), the
    // latter of which may call realloc().
    JS_ASSERT(numOperands() == 0);
#if DEBUG
    capacity_ = length;
#endif
    return inputs_.reserve(length);
}

void
MPhi::addInput(MDefinition *ins)
{
    // This can only been done if the length was reserved through reserveLength,
    // else the slower addInputSlow need to get called.
    JS_ASSERT(inputs_.length() < capacity_);

    uint32_t index = inputs_.length();
    inputs_.append(MUse());
    MPhi::setOperand(index, ins);
}

bool
MPhi::addInputSlow(MDefinition *ins)
{
    // The list of inputs to an MPhi is given as a vector of MUse nodes,
    // each of which is in the list of the producer MDefinition.
    // Because appending to a vector may reallocate the vector, it is possible
    // that this operation may cause the producers' linked lists to reference
    // invalid memory. Therefore, in the event of moving reallocation, each
    // MUse must be removed and reinserted from/into its producer's use chain.
    uint32_t index = inputs_.length();
    bool performingRealloc = !inputs_.canAppendWithoutRealloc(1);

    // Remove all MUses from all use lists, in case realloc() moves.
    if (performingRealloc) {
        for (uint32_t i = 0; i < index; i++) {
            MUse *use = &inputs_[i];
            use->producer()->removeUse(use);
        }
    }

    // Insert the new input.
    if (!inputs_.append(MUse()))
        return false;
    MPhi::setOperand(index, ins);

    // Add all previously-removed MUses back.
    if (performingRealloc) {
        for (uint32_t i = 0; i < index; i++) {
            MUse *use = &inputs_[i];
            use->producer()->addUse(use);
        }
    }

    return true;
}

uint32_t
MPrepareCall::argc() const
{
    JS_ASSERT(useCount() == 1);
    MCall *call = usesBegin()->consumer()->toDefinition()->toCall();
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
    setOperand(argnum + NumNonArgumentOperands, arg->toDefinition());
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
MBinaryBitwiseInstruction::specializeForAsmJS()
{
    specialization_ = MIRType_Int32;
    JS_ASSERT(type() == MIRType_Int32);
    setCommutative();
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
        if (use->consumer()->isResumePoint())
            continue;

        MDefinition *use_def = use->consumer()->toDefinition();
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
    return !implicitTruncate_ && (!range() || !range()->isInt32());
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
        canBeDivideByZero_ = false;

    // If lhs is a constant int != INT32_MIN, then
    // negative overflow check can be skipped.
    if (lhs()->isConstant() && !lhs()->toConstant()->value().isInt32(INT32_MIN))
        canBeNegativeOverflow_ = false;

    // If rhs is a constant int != -1, likewise.
    if (rhs()->isConstant() && !rhs()->toConstant()->value().isInt32(-1))
        canBeNegativeOverflow_ = false;

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

bool
MDiv::fallible()
{
    return !isTruncated();
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

    return this;
}

bool
MMod::fallible()
{
    return !isTruncated();
}

bool
MAdd::fallible()
{
    // the add is fallible if range analysis does not say that it is finite, AND
    // either the truncation analysis shows that there are non-truncated uses.
    if (isTruncated())
        return false;
    if (range() && range()->isInt32())
        return false;
    return true;
}

bool
MSub::fallible()
{
    // see comment in MAdd::fallible()
    if (isTruncated())
        return false;
    if (range() && range()->isInt32())
        return false;
    return true;
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

bool
MMul::updateForReplacement(MDefinition *ins_)
{
    MMul *ins = ins_->toMul();
    bool negativeZero = canBeNegativeZero() || ins->canBeNegativeZero();
    setCanBeNegativeZero(negativeZero);
    return true;
}

bool
MMul::canOverflow()
{
    if (isTruncated())
        return false;
    return !range() || !range()->isInt32();
}

void
MBinaryArithInstruction::infer(const TypeOracle::BinaryTypes &b, JSContext *cx)
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

static bool
CanDoValueBitwiseCmp(JSContext *cx, types::StackTypeSet *lhs, types::StackTypeSet *rhs, bool looseEq)
{
    // Only primitive (not double/string) or objects are supported.
    // I.e. Undefined/Null/Boolean/Int32 and Object
    if (!lhs->knownPrimitiveOrObject() ||
        lhs->hasAnyFlag(types::TYPE_FLAG_STRING) ||
        lhs->hasAnyFlag(types::TYPE_FLAG_DOUBLE) ||
        !rhs->knownPrimitiveOrObject() ||
        rhs->hasAnyFlag(types::TYPE_FLAG_STRING) ||
        rhs->hasAnyFlag(types::TYPE_FLAG_DOUBLE))
    {
        return false;
    }

    // Objects that emulate undefined are not supported.
    if (lhs->maybeObject() &&
        lhs->hasObjectFlags(cx, types::OBJECT_FLAG_EMULATES_UNDEFINED))
    {
        return false;
    }
    if (rhs->maybeObject() &&
        rhs->hasObjectFlags(cx, types::OBJECT_FLAG_EMULATES_UNDEFINED))
    {
        return false;
    }

    // In the loose comparison more values could be the same,
    // but value comparison reporting otherwise.
    if (looseEq) {

        // Undefined compared loosy to Null is not supported,
        // because tag is different, but value can be the same (undefined == null).
        if ((lhs->hasAnyFlag(types::TYPE_FLAG_UNDEFINED) &&
             rhs->hasAnyFlag(types::TYPE_FLAG_NULL)) ||
            (lhs->hasAnyFlag(types::TYPE_FLAG_NULL) &&
             rhs->hasAnyFlag(types::TYPE_FLAG_UNDEFINED)))
        {
            return false;
        }

        // Int32 compared loosy to Boolean is not supported,
        // because tag is different, but value can be the same (1 == true).
        if ((lhs->hasAnyFlag(types::TYPE_FLAG_INT32) &&
             rhs->hasAnyFlag(types::TYPE_FLAG_BOOLEAN)) ||
            (lhs->hasAnyFlag(types::TYPE_FLAG_BOOLEAN) &&
             rhs->hasAnyFlag(types::TYPE_FLAG_INT32)))
        {
            return false;
        }

        // For loosy comparison of an object with a Boolean/Number/String
        // the valueOf the object is taken. Therefore not supported.
        types::TypeFlags numbers = types::TYPE_FLAG_BOOLEAN |
                                   types::TYPE_FLAG_INT32;
        if ((lhs->maybeObject() && rhs->hasAnyFlag(numbers)) ||
            (rhs->maybeObject() && lhs->hasAnyFlag(numbers)))
        {
            return false;
        }
    }

    return true;
}

MIRType
MCompare::inputType()
{
    switch(compareType_) {
      case Compare_Undefined:
        return MIRType_Undefined;
      case Compare_Null:
        return MIRType_Null;
      case Compare_Boolean:
        return MIRType_Boolean;
      case Compare_UInt32:
      case Compare_Int32:
        return MIRType_Int32;
      case Compare_Double:
        return MIRType_Double;
      case Compare_String:
      case Compare_StrictString:
        return MIRType_String;
      case Compare_Object:
        return MIRType_Object;
      case Compare_Unknown:
      case Compare_Value:
        return MIRType_Value;
      default:
        JS_NOT_REACHED("No known conversion");
        return MIRType_None;
    }
}

void
MCompare::infer(const TypeOracle::BinaryTypes &b, JSContext *cx)
{
    if (!b.lhsTypes || !b.rhsTypes)
        return;

    JS_ASSERT(operandMightEmulateUndefined());

    if (!MaybeEmulatesUndefined(b.lhsTypes, cx) && !MaybeEmulatesUndefined(b.rhsTypes, cx))
        markNoOperandEmulatesUndefined();

    MIRType lhs = MIRTypeFromValueType(b.lhsTypes->getKnownTypeTag());
    MIRType rhs = MIRTypeFromValueType(b.rhsTypes->getKnownTypeTag());

    bool looseEq = jsop() == JSOP_EQ || jsop() == JSOP_NE;
    bool strictEq = jsop() == JSOP_STRICTEQ || jsop() == JSOP_STRICTNE;
    bool relationalEq = !(looseEq || strictEq);

    // Integer to integer or boolean to boolean comparisons may be treated as Int32.
    if ((lhs == MIRType_Int32 && rhs == MIRType_Int32) ||
        (lhs == MIRType_Boolean && rhs == MIRType_Boolean))
    {
        compareType_ = Compare_Int32;
        return;
    }

    // Loose/relational cross-integer/boolean comparisons may be treated as Int32.
    if (!strictEq &&
        (lhs == MIRType_Int32 || lhs == MIRType_Boolean) &&
        (rhs == MIRType_Int32 || rhs == MIRType_Boolean))
    {
        compareType_ = Compare_Int32;
        return;
    }

    // Numeric comparisons against a double coerce to double.
    if (IsNumberType(lhs) && IsNumberType(rhs)) {
        compareType_ = Compare_Double;
        return;
    }

    // Any comparison is allowed except strict eq.
    if (!strictEq &&
        ((lhs == MIRType_Double && SafelyCoercesToDouble(cx, b.rhsTypes)) ||
         (rhs == MIRType_Double && SafelyCoercesToDouble(cx, b.lhsTypes))))
    {
        compareType_ = Compare_Double;
        return;
    }

    // Handle object comparison.
    if (!relationalEq && lhs == MIRType_Object && rhs == MIRType_Object) {
        compareType_ = Compare_Object;
        return;
    }

    // Handle string comparisons. (Relational string compares are still unsupported).
    if (!relationalEq && lhs == MIRType_String && rhs == MIRType_String) {
        compareType_ = Compare_String;
        return;
    }

    if (strictEq && lhs == MIRType_String) {
        // Lowering expects the rhs to be definitly string.
        compareType_ = Compare_StrictString;
        swapOperands();
        return;
    }

    if (strictEq && rhs == MIRType_String) {
        compareType_ = Compare_StrictString;
        return;
    }

    // Handle compare with lhs being Undefined or Null.
    if (!relationalEq && IsNullOrUndefined(lhs)) {
        // Lowering expects the rhs to be null/undefined, so we have to
        // swap the operands. This is necessary since we may not know which
        // operand was null/undefined during lowering (both operands may have
        // MIRType_Value).
        compareType_ = (lhs == MIRType_Null) ? Compare_Null : Compare_Undefined;
        swapOperands();
        return;
    }

    // Handle compare with rhs being Undefined or Null.
    if (!relationalEq && IsNullOrUndefined(rhs)) {
        compareType_ = (rhs == MIRType_Null) ? Compare_Null : Compare_Undefined;
        return;
    }

    // Handle strict comparison with lhs/rhs being typed Boolean.
    if (strictEq && (lhs == MIRType_Boolean || rhs == MIRType_Boolean)) {
        // bool/bool case got an int32 specialization earlier.
        JS_ASSERT(!(lhs == MIRType_Boolean && rhs == MIRType_Boolean));

        // Ensure the boolean is on the right so that the type policy knows
        // which side to unbox.
        if (lhs == MIRType_Boolean)
             swapOperands();

        compareType_ = Compare_Boolean;
        return;
    }

    // Determine if we can do the compare based on a quick value check.
    if (!relationalEq && CanDoValueBitwiseCmp(cx, b.lhsTypes, b.rhsTypes, looseEq)) {
        compareType_ = Compare_Value;
        return;
    }
}

MBitNot *
MBitNot::New(MDefinition *input)
{
    return new MBitNot(input);
}

MBitNot *
MBitNot::NewAsmJS(MDefinition *input)
{
    MBitNot *ins = new MBitNot(input);
    ins->specialization_ = MIRType_Int32;
    JS_ASSERT(ins->type() == MIRType_Int32);
    return ins;
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

    JSRuntime *rt = GetIonContext()->runtime;
    return MConstant::New(StringValue(TypeName(type, rt)));
}

MBitAnd *
MBitAnd::New(MDefinition *left, MDefinition *right)
{
    return new MBitAnd(left, right);
}

MBitAnd *
MBitAnd::NewAsmJS(MDefinition *left, MDefinition *right)
{
    MBitAnd *ins = new MBitAnd(left, right);
    ins->specializeForAsmJS();
    return ins;
}

MBitOr *
MBitOr::New(MDefinition *left, MDefinition *right)
{
    return new MBitOr(left, right);
}

MBitOr *
MBitOr::NewAsmJS(MDefinition *left, MDefinition *right)
{
    MBitOr *ins = new MBitOr(left, right);
    ins->specializeForAsmJS();
    return ins;
}

MBitXor *
MBitXor::New(MDefinition *left, MDefinition *right)
{
    return new MBitXor(left, right);
}

MBitXor *
MBitXor::NewAsmJS(MDefinition *left, MDefinition *right)
{
    MBitXor *ins = new MBitXor(left, right);
    ins->specializeForAsmJS();
    return ins;
}

MLsh *
MLsh::New(MDefinition *left, MDefinition *right)
{
    return new MLsh(left, right);
}

MLsh *
MLsh::NewAsmJS(MDefinition *left, MDefinition *right)
{
    MLsh *ins = new MLsh(left, right);
    ins->specializeForAsmJS();
    return ins;
}

MRsh *
MRsh::New(MDefinition *left, MDefinition *right)
{
    return new MRsh(left, right);
}

MRsh *
MRsh::NewAsmJS(MDefinition *left, MDefinition *right)
{
    MRsh *ins = new MRsh(left, right);
    ins->specializeForAsmJS();
    return ins;
}

MUrsh *
MUrsh::New(MDefinition *left, MDefinition *right)
{
    return new MUrsh(left, right);
}

MUrsh *
MUrsh::NewAsmJS(MDefinition *left, MDefinition *right)
{
    MUrsh *ins = new MUrsh(left, right);
    ins->specializeForAsmJS();
    ins->canOverflow_ = false;
    return ins;
}

MResumePoint *
MResumePoint::New(MBasicBlock *block, jsbytecode *pc, MResumePoint *parent, Mode mode)
{
    MResumePoint *resume = new MResumePoint(block, pc, parent, mode);
    if (!resume->init())
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

void
MResumePoint::inherit(MBasicBlock *block)
{
    for (size_t i = 0; i < stackDepth(); i++) {
        MDefinition *def = block->getSlot(i);
        // We have to unwrap MPassArg: it's removed when inlining calls
        // and LStackArg does not define a value.
        if (def->isPassArg())
            def = def->toPassArg()->getArgument();
        setOperand(i, def);
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
        int32_t ret = ToInt32(v.toDouble());
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

    if (compareType_ == Compare_Null || compareType_ == Compare_Undefined) {
        JS_ASSERT(op == JSOP_EQ || op == JSOP_STRICTEQ ||
                  op == JSOP_NE || op == JSOP_STRICTNE);

        // The LHS is the value we want to test against null or undefined.
        switch (lhs()->type()) {
          case MIRType_Value:
            return false;
          case MIRType_Undefined:
          case MIRType_Null:
            if (lhs()->type() == inputType()) {
                // Both sides have the same type, null or undefined.
                *result = (op == JSOP_EQ || op == JSOP_STRICTEQ);
            } else {
                // One side is null, the other side is undefined. The result is only
                // true for loose equality.
                *result = (op == JSOP_EQ || op == JSOP_STRICTNE);
            }
            return true;
          case MIRType_Object:
            if ((op == JSOP_EQ || op == JSOP_NE) && operandMightEmulateUndefined())
                return false;
            /* FALL THROUGH */
          case MIRType_Int32:
          case MIRType_Double:
          case MIRType_String:
          case MIRType_Boolean:
            *result = (op == JSOP_NE || op == JSOP_STRICTNE);
            return true;
          default:
            JS_NOT_REACHED("Unexpected type");
            return false;
        }
    }

    if (compareType_ == Compare_Boolean) {
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

    if (compareType_ == Compare_StrictString) {
        JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);
        JS_ASSERT(rhs()->type() == MIRType_String);

        switch (lhs()->type()) {
          case MIRType_Value:
            return false;
          case MIRType_Boolean:
          case MIRType_Int32:
          case MIRType_Double:
          case MIRType_Object:
          case MIRType_Null:
          case MIRType_Undefined:
            *result = (op == JSOP_STRICTNE);
            return true;
          case MIRType_String:
            // Compare_String specialization should handle this.
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
    if (type() != MIRType_Boolean && type() != MIRType_Int32)
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

    if (compareType_ == Compare_UInt32) {
        uint32_t lhsUint = uint32_t(lhs.toInt32());
        uint32_t rhsUint = uint32_t(rhs.toInt32());

        switch (jsop_) {
          case JSOP_LT:
            *result = (lhsUint < rhsUint);
            break;
          case JSOP_LE:
            *result = (lhsUint <= rhsUint);
            break;
          case JSOP_GT:
            *result = (lhsUint > rhsUint);
            break;
          case JSOP_GE:
            *result = (lhsUint >= rhsUint);
            break;
          case JSOP_EQ:
            *result = (lhsUint == rhsUint);
            break;
          case JSOP_NE:
            *result = (lhsUint != rhsUint);
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

    if (tryFold(&result) || evaluateConstantOperands(&result)) {
        if (type() == MIRType_Int32)
            return MConstant::New(Int32Value(result));

        JS_ASSERT(type() == MIRType_Boolean);
        return MConstant::New(BooleanValue(result));
    }

    return this;
}

void
MNot::infer(const TypeOracle::UnaryTypes &u, JSContext *cx)
{
    if (!u.inTypes)
        return;

    JS_ASSERT(operandMightEmulateUndefined());

    if (!MaybeEmulatesUndefined(u.inTypes, cx))
        markOperandCantEmulateUndefined();
}

MDefinition *
MNot::foldsTo(bool useValueNumbers)
{
    // Fold if the input is constant
    if (operand()->isConstant()) {
        const Value &v = operand()->toConstant()->value();
        if (type() == MIRType_Int32)
            return MConstant::New(Int32Value(!ToBoolean(v)));

        // ToBoolean can cause no side effects, so this is safe.
        return MConstant::New(BooleanValue(!ToBoolean(v)));
    }

    // NOT of an undefined or null value is always true
    if (operand()->type() == MIRType_Undefined || operand()->type() == MIRType_Null)
        return MConstant::New(BooleanValue(true));

    // NOT of an object that can't emulate undefined is always false.
    if (operand()->type() == MIRType_Object && !operandMightEmulateUndefined())
        return MConstant::New(BooleanValue(false));

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

bool
MNewObject::shouldUseVM() const
{
    return templateObject()->hasSingletonType() ||
           templateObject()->hasDynamicSlots();
}

bool
MNewArray::shouldUseVM() const
{
    JS_ASSERT(count() < JSObject::NELEMENTS_LIMIT);

    size_t maxArraySlots =
        gc::GetGCKindSlots(gc::FINALIZE_OBJECT_LAST) - ObjectElements::VALUES_PER_HEADER;

    // Allocate space using the VMCall
    // when mir hints it needs to get allocated immediatly,
    // but only when data doesn't fit the available array slots.
    bool allocating = isAllocating() && count() > maxArraySlots;

    return templateObject()->hasSingletonType() || allocating;
}

bool
MLoadFixedSlot::mightAlias(MDefinition *store)
{
    if (store->isStoreFixedSlot() && store->toStoreFixedSlot()->slot() != slot())
        return false;
    return true;
}

bool
MLoadSlot::mightAlias(MDefinition *store)
{
    if (store->isStoreSlot() && store->toStoreSlot()->slot() != slot())
        return false;
    return true;
}

void
InlinePropertyTable::trimTo(AutoObjectVector &targets, Vector<bool> &choiceSet)
{
    for (size_t i = 0; i < targets.length(); i++) {
        // If the target was inlined, don't erase the entry.
        if (choiceSet[i])
            continue;

        JSFunction *target = targets[i]->toFunction();

        // Eliminate all entries containing the vetoed function from the map.
        size_t j = 0;
        while (j < numEntries()) {
            if (entries_[j]->func == target)
                entries_.erase(&entries_[j]);
            else
                j++;
        }
    }
}

void
InlinePropertyTable::trimToAndMaybePatchTargets(AutoObjectVector &targets,
                                                AutoObjectVector &originals)
{
    IonSpew(IonSpew_Inlining, "Got inlineable property cache with %d cases",
            (int)numEntries());

    size_t i = 0;
    while (i < numEntries()) {
        bool foundFunc = false;
        // Compare using originals, but if we find a matching function,
        // patch it to the target, which might be a clone.
        for (size_t j = 0; j < originals.length(); j++) {
            if (entries_[i]->func == originals[j]) {
                if (entries_[i]->func != targets[j])
                    entries_[i] = new Entry(entries_[i]->typeObj, targets[j]->toFunction());
                foundFunc = true;
                break;
            }
        }
        if (!foundFunc)
            entries_.erase(&(entries_[i]));
        else
            i++;
    }

    IonSpew(IonSpew_Inlining, "%d inlineable cases left after trimming to %d targets",
            (int)numEntries(), (int)targets.length());
}

bool
InlinePropertyTable::hasFunction(JSFunction *func) const
{
    for (size_t i = 0; i < numEntries(); i++) {
        if (entries_[i]->func == func)
            return true;
    }
    return false;
}

MDefinition *
MAsmJSUnsignedToDouble::foldsTo(bool useValueNumbers)
{
    if (input()->isConstant()) {
        const Value &v = input()->toConstant()->value();
        if (v.isInt32())
            return MConstant::New(DoubleValue(uint32_t(v.toInt32())));
    }

    return this;
}

MAsmJSCall *
MAsmJSCall::New(Callee callee, const Args &args, MIRType resultType, size_t spIncrement)
{
    MAsmJSCall *call = new MAsmJSCall;
    call->spIncrement_ = spIncrement;
    call->callee_ = callee;
    call->setResultType(resultType);

    call->numArgs_ = args.length();
    call->argRegs_ = (AnyRegister *)GetIonContext()->temp->allocate(call->numArgs_ * sizeof(AnyRegister));
    if (!call->argRegs_)
        return NULL;
    for (size_t i = 0; i < call->numArgs_; i++)
        call->argRegs_[i] = args[i].reg;

    call->numOperands_ = call->numArgs_ + (callee.which() == Callee::Dynamic ? 1 : 0);
    call->operands_ = (MUse *)GetIonContext()->temp->allocate(call->numOperands_ * sizeof(MUse));
    if (!call->operands_)
        return NULL;
    for (size_t i = 0; i < call->numArgs_; i++)
        call->setOperand(i, args[i].def);
    if (callee.which() == Callee::Dynamic)
        call->setOperand(call->numArgs_, callee.dynamic());

    return call;
}

