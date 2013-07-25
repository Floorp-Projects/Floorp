/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_PerfSpewer_h
#define ion_PerfSpewer_h

#include <stdio.h>

#include "jsscript.h"

#include "ion/IonMacroAssembler.h"
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
  protected:
    static uint32_t nextFunctionIndex;
    FILE *fp_;

  public:
    struct Record {
        const char *filename;
        unsigned lineNumber;
        unsigned columnNumber;
        uint32_t id;
        Label start, end;
        unsigned startOffset, endOffset;

        Record(const char *filename,
               unsigned lineNumber,
               unsigned columnNumber,
               uint32_t id)
          : filename(filename), lineNumber(lineNumber),
            columnNumber(columnNumber), id(id),
            startOffset(0u), endOffset(0u)
        {}
    };

    typedef Vector<Record, 1, SystemAllocPolicy> BasicBlocksVector;
  protected:
    BasicBlocksVector basicBlocks_;

  public:
    PerfSpewer();
    virtual ~PerfSpewer();

    virtual bool startBasicBlock(MBasicBlock *blk, MacroAssembler &masm);
    bool endBasicBlock(MacroAssembler &masm);
    void writeProfile(JSScript *script,
                      IonCode *code,
                      MacroAssembler &masm);
};

class AsmJSPerfSpewer : public PerfSpewer
{
  public:
    bool startBasicBlock(MBasicBlock *blk, MacroAssembler &masm);

    void noteBlocksOffsets(MacroAssembler &masm);
    BasicBlocksVector &basicBlocks() { return basicBlocks_; }

    void writeBlocksMap(unsigned long baseAddress, unsigned long funcStartOffset,
                        unsigned long funcSize, const char *filename, const char *funcName,
                        const BasicBlocksVector &basicBlocks);
    void writeFunctionMap(unsigned long base, unsigned long size, const char *filename,
                          unsigned lineno, unsigned colIndex, const char *funcName);
};

} // namespace ion
} // namespace js

#endif /* ion_PerfSpewer_h */
