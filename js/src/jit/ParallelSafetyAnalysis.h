/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ParallelSafetyAnalysis_h
#define jit_ParallelSafetyAnalysis_h

#include "jit/MIR.h"

namespace js {

class InterpreterFrame;

namespace jit {

class MIRGraph;
class AutoDestroyAllocator;

// Determines whether a function is compatible for parallel execution.
// Removes basic blocks containing unsafe MIR operations from the
// graph and replaces them with MBail blocks.
class ParallelSafetyAnalysis
{
    MIRGenerator *mir_;
    MIRGraph &graph_;

  public:
    ParallelSafetyAnalysis(MIRGenerator *mir,
                           MIRGraph &graph)
      : mir_(mir),
        graph_(graph)
    {}

    bool analyze();
};

// Code to collect list of possible call targets by scraping through
// TI and baseline data. Used to permit speculative transitive
// compilation in vm/ForkJoin.
//
// This code may clone scripts and thus may invoke the GC.  Hence only
// run from the link phase, which executes on the main thread.
typedef Vector<JSScript *, 4, IonAllocPolicy> CallTargetVector;
bool AddPossibleCallees(JSContext *cx, MIRGraph &graph, CallTargetVector &targets);

} // namespace jit
} // namespace js

#endif /* jit_ParallelSafetyAnalysis_h */
