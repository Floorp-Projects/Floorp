/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_CodeCoverage_h
#define vm_CodeCoverage_h

#include "mozilla/Vector.h"

#include "ds/LifoAlloc.h"

#include "vm/Printer.h"

struct JSCompartment;
class JSScript;

namespace js {
namespace coverage {

class LCovCompartment;

class LCovSource
{
  public:
    explicit LCovSource(LifoAlloc* alloc);

    // Visit all JSScript in pre-order, and collect the lcov output based on
    // the ScriptCounts counters.
    //
    // In case of the where this function is called during the finalization,
    // this assumes that all of the children scripts are still alive, and
    // not finalized yet.
    bool writeTopLevelScript(JSScript* script);

    // Write the Lcov output in a buffer, such as the one associated with
    // the runtime code coverage trace file.
    void exportInto(GenericPrinter& out) const;

  private:
    // Write the script name in out.
    bool writeSourceFilename(LSprinter& out, JSScript* script);

    // Write the script name in out.
    bool writeScriptName(LSprinter& out, JSScript* script);

    // Iterate over the bytecode and collect the lcov output based on the
    // ScriptCounts counters.
    bool writeScript(JSScript* script);

  private:
    // LifoAlloc string which hold the filename of the source.
    LSprinter outSF_;

    // LifoAlloc strings which hold the filename of each function as
    // well as the number of hits for each function.
    LSprinter outFN_;
    LSprinter outFNDA_;
    size_t numFunctionsFound_;
    size_t numFunctionsHit_;

    // LifoAlloc string which hold branches statistics.
    LSprinter outBRDA_;
    size_t numBranchesFound_;
    size_t numBranchesHit_;

    // LifoAlloc string which hold lines statistics.
    LSprinter outDA_;
    size_t numLinesInstrumented_;
    size_t numLinesHit_;
};

class LCovCompartment
{
  public:
    LCovCompartment();

    // Collect code coverage information
    void collectCodeCoverageInfo(JSCompartment* comp, JSScript* topLevel);

    // Write the Lcov output in a buffer, such as the one associated with
    // the runtime code coverage trace file.
    void exportInto(GenericPrinter& out) const;

  private:
    // Write the script name in out.
    bool writeCompartmentName(JSCompartment* comp);

  private:
    typedef Vector<LCovSource, 16, LifoAllocPolicy<Fallible>> LCovSourceVector;

    // LifoAlloc backend for all temporary allocations needed to stash the
    // strings to be written in the file.
    LifoAlloc alloc_;

    // LifoAlloc string which hold the name of the compartment.
    LSprinter outTN_;

    // Vector of all sources which are used in this compartment.
    LCovSourceVector* sources_;
};

} // namespace coverage
} // namespace js

#endif // vm_Printer_h

