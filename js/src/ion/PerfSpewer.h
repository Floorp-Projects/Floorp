/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ion_perfspewer_h__
#define js_ion_perfspewer_h__

#include <stdio.h>

#include "jsscript.h"
#include "IonMacroAssembler.h"
#include "js/RootingAPI.h"

class JSScript;

namespace js {
namespace ion {

class MBasicBlock;
class MacroAssembler;

#ifdef JS_ION_PERF
void CheckPerf();
bool PerfBlockEnabled();
bool PerfFuncEnabled();
static inline bool PerfEnabled() {
    return PerfBlockEnabled() || PerfFuncEnabled();
}
#else
static inline void CheckPerf() {}
static inline bool PerfBlockEnabled() { return false; }
static inline bool PerfFuncEnabled() { return false; }
static inline bool PerfEnabled() { return false; }
#endif

class PerfSpewer
{
  private:
    static uint32_t nextFunctionIndex;

    struct Record {
        const char *filename;
        unsigned lineNumber;
        unsigned columnNumber;
        uint32_t id;
        Label start, end;

        Record(const char *filename,
               unsigned lineNumber,
               unsigned columnNumber,
               uint32_t id)
          : filename(filename), lineNumber(lineNumber),
            columnNumber(columnNumber), id(id)
        {}
    };

    FILE *fp_;
    Vector<Record, 1, SystemAllocPolicy> basicBlocks_;

  public:
    PerfSpewer();
    ~PerfSpewer();

    bool init(const char *path);

    bool startBasicBlock(MBasicBlock *blk, MacroAssembler &masm);
    bool endBasicBlock(MacroAssembler &masm);
    void writeProfile(JSScript *script,
                      IonCode *code,
                      MacroAssembler &masm);
};

} // namespace ion
} // namespace js

#endif // js_ion_perfspewer_h__

