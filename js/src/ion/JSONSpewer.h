/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ion_jsonspewer_h__
#define js_ion_jsonspewer_h__

#include <stdio.h>

#include "gc/Root.h"
#include "jsscript.h"

class JSScript;

namespace js {
namespace ion {

class MDefinition;
class MInstruction;
class MBasicBlock;
class MIRGraph;
class MResumePoint;
class LinearScanAllocator;
class LInstruction;

class JSONSpewer
{
  private:
    // Set by beginFunction(); unset by endFunction().
    // Used to correctly format output in case of abort during compilation.
    bool inFunction_;

    int indentLevel_;
    bool first_;
    FILE *fp_;

    void indent();

    void property(const char *name);
    void beginObject();
    void beginObjectProperty(const char *name);
    void beginListProperty(const char *name);
    void stringValue(const char *format, ...);
    void stringProperty(const char *name, const char *format, ...);
    void integerValue(int value);
    void integerProperty(const char *name, int value);
    void endObject();
    void endList();

  public:
    JSONSpewer()
      : inFunction_(false),
        indentLevel_(0),
        first_(true),
        fp_(NULL)
    { }
    ~JSONSpewer();

    bool init(const char *path);
    void beginFunction(UnrootedScript script);
    void beginPass(const char * pass);
    void spewMDef(MDefinition *def);
    void spewMResumePoint(MResumePoint *rp);
    void spewMIR(MIRGraph *mir);
    void spewLIns(LInstruction *ins);
    void spewLIR(MIRGraph *mir);
    void spewIntervals(LinearScanAllocator *regalloc);
    void endPass();
    void endFunction();
    void finish();
};

} // namespace ion
} // namespace js

#endif // js_ion_jsonspewer_h__

