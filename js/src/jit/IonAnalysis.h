/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonAnalysis_h
#define jit_IonAnalysis_h

// This file declares various analysis passes that operate on MIR.

#include "jit/IonAllocPolicy.h"
#include "jit/MIR.h"

namespace js {
namespace jit {

class MIRGenerator;
class MIRGraph;

void
FoldTests(MIRGraph &graph);

bool
SplitCriticalEdges(MIRGraph &graph);

enum Observability {
    ConservativeObservability,
    AggressiveObservability
};

bool
EliminatePhis(MIRGenerator *mir, MIRGraph &graph, Observability observe);

size_t
MarkLoopBlocks(MIRGraph &graph, MBasicBlock *header, bool *canOsr);

void
UnmarkLoopBlocks(MIRGraph &graph, MBasicBlock *header);

bool
MakeLoopsContiguous(MIRGraph &graph);

bool
EliminateDeadResumePointOperands(MIRGenerator *mir, MIRGraph &graph);

bool
EliminateDeadCode(MIRGenerator *mir, MIRGraph &graph);

bool
ApplyTypeInformation(MIRGenerator *mir, MIRGraph &graph);

bool
MakeMRegExpHoistable(MIRGraph &graph);

bool
RenumberBlocks(MIRGraph &graph);

bool
AccountForCFGChanges(MIRGenerator *mir, MIRGraph &graph, bool updateAliasAnalysis);

bool
RemoveUnmarkedBlocks(MIRGenerator *mir, MIRGraph &graph, uint32_t numMarkedBlocks);

bool
BuildDominatorTree(MIRGraph &graph);

bool
BuildPhiReverseMapping(MIRGraph &graph);

void
AssertBasicGraphCoherency(MIRGraph &graph);

void
AssertGraphCoherency(MIRGraph &graph);

void
AssertExtendedGraphCoherency(MIRGraph &graph);

bool
EliminateRedundantChecks(MIRGraph &graph);

class MDefinition;

// Simple linear sum of the form 'n' or 'x + n'.
struct SimpleLinearSum
{
    MDefinition *term;
    int32_t constant;

    SimpleLinearSum(MDefinition *term, int32_t constant)
        : term(term), constant(constant)
    {}
};

SimpleLinearSum
ExtractLinearSum(MDefinition *ins);

bool
ExtractLinearInequality(MTest *test, BranchDirection direction,
                        SimpleLinearSum *plhs, MDefinition **prhs, bool *plessEqual);

struct LinearTerm
{
    MDefinition *term;
    int32_t scale;

    LinearTerm(MDefinition *term, int32_t scale)
      : term(term), scale(scale)
    {
    }
};

// General linear sum of the form 'x1*n1 + x2*n2 + ... + n'
class LinearSum
{
  public:
    explicit LinearSum(TempAllocator &alloc)
      : terms_(alloc),
        constant_(0)
    {
    }

    LinearSum(const LinearSum &other)
      : terms_(other.terms_.allocPolicy()),
        constant_(other.constant_)
    {
        terms_.appendAll(other.terms_);
    }

    bool multiply(int32_t scale);
    bool add(const LinearSum &other);
    bool add(MDefinition *term, int32_t scale);
    bool add(int32_t constant);

    int32_t constant() const { return constant_; }
    size_t numTerms() const { return terms_.length(); }
    LinearTerm term(size_t i) const { return terms_[i]; }

    void print(Sprinter &sp) const;
    void dump(FILE *) const;
    void dump() const;

  private:
    Vector<LinearTerm, 2, IonAllocPolicy> terms_;
    int32_t constant_;
};

bool
AnalyzeNewScriptProperties(JSContext *cx, JSFunction *fun,
                           types::TypeObject *type, HandleObject baseobj,
                           Vector<types::TypeNewScript::Initializer> *initializerList);

bool
AnalyzeArgumentsUsage(JSContext *cx, JSScript *script);

} // namespace jit
} // namespace js

#endif /* jit_IonAnalysis_h */
