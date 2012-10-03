/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "Ion.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "RangeAnalysis.h"
#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

// This algorithm is based on the paper "Eliminating Range Checks Using
// Static Single Assignment Form" by Gough and Klaren.
//
// We associate a range object with each SSA name, and the ranges are consulted
// in order to determine whether overflow is possible for arithmetic
// computations.
//
// An important source of range information that requires care to take
// advantage of is conditional control flow. Consider the code below:
//
// if (x < 0) {
//   y = x + 2000000000;
// } else {
//   if (x < 1000000000) {
//     y = x * 2;
//   } else {
//     y = x - 3000000000;
//   }
// }
//
// The arithmetic operations in this code cannot overflow, but it is not
// sufficient to simply associate each name with a range, since the information
// differs between basic blocks. The traditional dataflow approach would be
// associate ranges with (name, basic block) pairs. This solution is not
// satisfying, since we lose the benefit of SSA form: in SSA form, each
// definition has a unique name, so there is no need to track information about
// the control flow of the program.
//
// The approach used here is to add a new form of pseudo operation called a
// beta node, which associates range information with a value. These beta
// instructions take one argument and additionally have an auxiliary constant
// range associated with them. Operationally, beta nodes are just copies, but
// the invariant expressed by beta node copies is that the output will fall
// inside the range given by the beta node.  Gough and Klaeren refer to SSA
// extended with these beta nodes as XSA form. The following shows the example
// code transformed into XSA form:
//
// if (x < 0) {
//   x1 = Beta(x, [INT_MIN, -1]);
//   y1 = x1 + 2000000000;
// } else {
//   x2 = Beta(x, [0, INT_MAX]);
//   if (x2 < 1000000000) {
//     x3 = Beta(x2, [INT_MIN, 999999999]);
//     y2 = x3*2;
//   } else {
//     x4 = Beta(x2, [1000000000, INT_MAX]);
//     y3 = x4 - 3000000000;
//   }
//   y4 = Phi(y2, y3);
// }
// y = Phi(y1, y4);
//
// We insert beta nodes for the purposes of range analysis (they might also be
// usefully used for other forms of bounds check elimination) and remove them
// after range analysis is performed. The remaining compiler phases do not ever
// encounter beta nodes.

RangeAnalysis::RangeAnalysis(MIRGraph &graph)
  : graph_(graph)
{
}

static bool
IsDominatedUse(MBasicBlock *block, MUse *use)
{
    MNode *n = use->node();
    bool isPhi = n->isDefinition() && n->toDefinition()->isPhi();

    if (isPhi)
        return block->dominates(n->block()->getPredecessor(use->index()));

    return block->dominates(n->block());
}

static inline void
SpewRange(MDefinition *def)
{
#ifdef DEBUG
    if (IonSpewEnabled(IonSpew_Range)) {
        IonSpewHeader(IonSpew_Range);
        fprintf(IonSpewFile, "%d has range ", def->id());
        def->range()->printRange(IonSpewFile);
        fprintf(IonSpewFile, "\n");
    }
#endif
}

void
RangeAnalysis::replaceDominatedUsesWith(MDefinition *orig, MDefinition *dom,
                                            MBasicBlock *block)
{
    for (MUseIterator i(orig->usesBegin()); i != orig->usesEnd(); ) {
        if (i->node() != dom && IsDominatedUse(block, *i))
            i = i->node()->replaceOperand(i, dom);
        else
            i++;
    }
}

bool
RangeAnalysis::addBetaNobes()
{
    IonSpew(IonSpew_Range, "Adding beta nobes");

    for (PostorderIterator i(graph_.poBegin()); i != graph_.poEnd(); i++) {
        MBasicBlock *block = *i;
        IonSpew(IonSpew_Range, "Looking at block %d", block->id());

        BranchDirection branch_dir;
        MTest *test = block->immediateDominatorBranch(&branch_dir);

        if (!test || !test->getOperand(0)->isCompare())
            continue;

        MCompare *compare = test->getOperand(0)->toCompare();

        MDefinition *left = compare->getOperand(0);
        MDefinition *right = compare->getOperand(1);
        int32 bound;
        MDefinition *val = NULL;

        JSOp jsop = compare->jsop();

        if (branch_dir == FALSE_BRANCH)
            jsop = analyze::NegateCompareOp(jsop);

        if (left->isConstant() && left->toConstant()->value().isInt32()) {
            bound = left->toConstant()->value().toInt32();
            val = right;
            jsop = analyze::ReverseCompareOp(jsop);
        } else if (right->isConstant() && right->toConstant()->value().isInt32()) {
            bound = right->toConstant()->value().toInt32();
            val = left;
        } else {
            MDefinition *smaller = NULL;
            MDefinition *greater = NULL;
            if (jsop == JSOP_LT) {
                smaller = left;
                greater = right;
            } else if (jsop == JSOP_GT) {
                smaller = right;
                greater = left;
            }
            if (smaller && greater) {
                MBeta *beta;
                beta = MBeta::New(smaller, Range(JSVAL_INT_MIN, JSVAL_INT_MAX-1));
                block->insertBefore(*block->begin(), beta);
                replaceDominatedUsesWith(smaller, beta, block);
                beta = MBeta::New(greater, Range(JSVAL_INT_MIN+1, JSVAL_INT_MAX));
                block->insertBefore(*block->begin(), beta);
                replaceDominatedUsesWith(greater, beta, block);
            }
            continue;
        }

        JS_ASSERT(val);


        Range comp;
        switch (jsop) {
          case JSOP_LE:
            comp.setUpper(bound);
            break;
          case JSOP_LT:
            if (!SafeSub(bound, 1, &bound))
                break;
            comp.setUpper(bound);
            break;
          case JSOP_GE:
            comp.setLower(bound);
            break;
          case JSOP_GT:
            if (!SafeAdd(bound, 1, &bound))
                break;
            comp.setLower(bound);
            break;
          case JSOP_EQ:
            comp.setLower(bound);
            comp.setUpper(bound);
          default:
            break; // well, for neq we could have
                   // [-\inf, bound-1] U [bound+1, \inf] but we only use contiguous ranges.
        }

        IonSpew(IonSpew_Range, "Adding beta node for %d", val->id());
        MBeta *beta = MBeta::New(val, comp);
        block->insertBefore(*block->begin(), beta);
        replaceDominatedUsesWith(val, beta, block);
    }

    return true;
}

bool
RangeAnalysis::removeBetaNobes()
{
    IonSpew(IonSpew_Range, "Removing beta nobes");

    for (PostorderIterator i(graph_.poBegin()); i != graph_.poEnd(); i++) {
        MBasicBlock *block = *i;
        for (MDefinitionIterator iter(*i); iter; ) {
            MDefinition *def = *iter;
            if (def->isBeta()) {
                MDefinition *op = def->getOperand(0);
                IonSpew(IonSpew_Range, "Removing beta node %d for %d",
                        def->id(), op->id());
                def->replaceAllUsesWith(op);
                iter = block->discardDefAt(iter);
            } else {
                // We only place Beta nodes at the beginning of basic
                // blocks, so if we see something else, we can move on
                // to the next block.
                break;
            }
        }
    }
    return true;
}

void
Range::printRange(FILE *fp)
{
    JS_ASSERT_IF(lower_infinite_, lower_ == JSVAL_INT_MIN);
    JS_ASSERT_IF(upper_infinite_, upper_ == JSVAL_INT_MAX);
    fprintf(fp, "[");
    if (lower_infinite_) { fprintf(fp, "-inf"); } else { fprintf(fp, "%d", lower_); }
    fprintf(fp, ", ");
    if (upper_infinite_) { fprintf(fp, "inf"); } else { fprintf(fp, "%d", upper_); }
    fprintf(fp, "]");
}

Range
Range::intersect(const Range *lhs, const Range *rhs, bool *nullRange)
{
    Range r(
        Max(lhs->lower_, rhs->lower_),
        Min(lhs->upper_, rhs->upper_));

    r.lower_infinite_ = lhs->lower_infinite_ && rhs->lower_infinite_;
    r.upper_infinite_ = lhs->upper_infinite_ && rhs->upper_infinite_;

    // :TODO: This information could be used better. If upper < lower, then we
    // have conflicting constraints. Consider:
    //
    // if (x < 0) {
    //   if (x > 0) {
    //     [Some code.]
    //   }
    // }
    //
    // In this case, the block is dead. Right now, we just disregard this fact
    // and make the range infinite, rather than empty.
    //
    // Instead, we should use it to eliminate the dead block.
    // (Bug 765127)
    if (r.upper_ < r.lower_) {
        *nullRange = true;
        r.makeRangeInfinite();
    }
    return r;
}

void
Range::unionWith(const Range *other)
{
   setLower(Min(lower_, other->lower_));
   lower_infinite_ |= other->lower_infinite_;
   setUpper(Max(upper_, other->upper_));
   upper_infinite_ |= other->upper_infinite_;
}

void
Range::unionWith(RangeChangeCount *other)
{
    if (other->lowerCount_ <= 2) {
        setLower(Min(lower_, other->oldRange.lower_));
        lower_infinite_ |= other->oldRange.lower_infinite_;
    } else {
        other->lowerCount_ = 0;
    }
    if (other->upperCount_ <= 2) {
        setUpper(Max(upper_, other->oldRange.upper_));
        upper_infinite_ |= other->oldRange.upper_infinite_;
    } else {
        other->upperCount_ = 0;
    }
}

Range
Range::add(const Range *lhs, const Range *rhs)
{
    Range ret(
        (int64_t)lhs->lower_ + (int64_t)rhs->lower_,
        (int64_t)lhs->upper_ + (int64_t)rhs->upper_);
    return ret;
}

Range
Range::sub(const Range *lhs, const Range *rhs)
{
    Range ret(
        (int64_t)lhs->lower_ - (int64_t)rhs->upper_,
        (int64_t)lhs->upper_ - (int64_t)rhs->lower_);
    return ret;

}
Range
Range::addTruncate(const Range *lhs, const Range *rhs)
{
    Range ret = Truncate((int64_t)lhs->lower_ + (int64_t)rhs->lower_,
                         (int64_t)lhs->upper_ + (int64_t)rhs->upper_);
    return ret;
}

Range
Range::subTruncate(const Range *lhs, const Range *rhs)
{
    Range ret = Truncate((int64_t)lhs->lower_ - (int64_t)rhs->upper_,
                         (int64_t)lhs->upper_ - (int64_t)rhs->lower_);
    return ret;
}

Range
Range::and_(const Range *lhs, const Range *rhs)
{
    uint64_t lower = 0;
    // If both numbers can be negative, issues can be had.
    if (lhs->lower_ < 0 && rhs->lower_ < 0)
        lower = INT_MIN;
    uint64_t upper = lhs->upper_;
    if (rhs->upper_ < lhs->upper_)
        upper = rhs->upper_;
    Range ret(lower, upper);
    return ret;

}
Range
Range::mul(const Range *lhs, const Range *rhs)
{
    int64_t a = (int64_t)lhs->lower_ * (int64_t)rhs->lower_;
    int64_t b = (int64_t)lhs->lower_ * (int64_t)rhs->upper_;
    int64_t c = (int64_t)lhs->upper_ * (int64_t)rhs->lower_;
    int64_t d = (int64_t)lhs->upper_ * (int64_t)rhs->upper_;
    Range ret(
        Min( Min(a, b), Min(c, d) ),
        Max( Max(a, b), Max(c, d) ));
    return ret;
}

Range
Range::shl(const Range *lhs, int32 c)
{
    int32 shift = c & 0x1f;
    Range ret(
        (int64_t)lhs->lower_ << shift,
        (int64_t)lhs->upper_ << shift);
    return ret;
}

Range
Range::shr(const Range *lhs, int32 c)
{
    int32 shift = c & 0x1f;
    Range ret(
        (int64_t)lhs->lower_ >> shift,
        (int64_t)lhs->upper_ >> shift);
    return ret;
}

bool
Range::update(const Range *other)
{
    bool changed =
        lower_ != other->lower_ ||
        lower_infinite_ != other->lower_infinite_ ||
        upper_ != other->upper_ ||
        upper_infinite_ != other->upper_infinite_;
    if (changed) {
        lower_ = other->lower_;
        lower_infinite_ = other->lower_infinite_;
        upper_ = other->upper_;
        upper_infinite_ = other->upper_infinite_;
    }

    return changed;
}

static inline bool
AddToWorklist(MDefinitionVector &worklist, MDefinition *def)
{
    if (def->isInWorklist())
        return true;
    def->setInWorklist();
    return worklist.append(def);
}

static inline MDefinition *
PopFromWorklist(MDefinitionVector &worklist)
{
    JS_ASSERT(!worklist.empty());
    MDefinition *def = worklist.popCopy();
    def->setNotInWorklist();
    return def;
}


bool
RangeAnalysis::analyze()
{
    for (PostorderIterator i(graph_.poBegin()); i != graph_.poEnd(); i++) {
        MBasicBlock *curBlock = *i;
        if (!curBlock->isLoopHeader())
            continue;
        for (MPhiIterator pi(curBlock->phisBegin()); pi != curBlock->phisEnd(); pi++)
            if (!pi->initCounts())
                return false;
    }

    IonSpew(IonSpew_Range, "Doing range propagation");
    MDefinitionVector worklist;

    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        for (MDefinitionIterator iter(*block); iter; iter++) {
            MDefinition *def = *iter;

            AddToWorklist(worklist, def);

        }
    }
    size_t iters = 0;

    while (!worklist.empty()) {
        MDefinition *def = PopFromWorklist(worklist);
        IonSpew(IonSpew_Range, "recomputing range on %d", def->id());
        SpewRange(def);
        if (!def->earlyAbortCheck() && def->recomputeRange()) {
            JS_ASSERT(def->range()->lower() <= def->range()->upper());
            IonSpew(IonSpew_Range, "Range changed; adding consumers");
            for (MUseDefIterator use(def); use; use++) {
                if(!AddToWorklist(worklist, use.def()))
                    return false;
            }
        }
        iters++;
    }
    // Cleanup (in case we stopped due to MAX_ITERS)
    for(size_t i = 0; i < worklist.length(); i++)
        worklist[i]->setNotInWorklist();


#ifdef DEBUG
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        for (MDefinitionIterator iter(*block); iter; iter++) {
            MDefinition *def = *iter;
            SpewRange(def);
            JS_ASSERT(def->range()->lower() <= def->range()->upper());
            JS_ASSERT(!def->isInWorklist());
        }
    }
#endif
    return true;
}
