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

#ifdef DEBUG

#include <stdarg.h>
#include "IonBuilder.h"
#include "Ion.h"
#include "C1Spewer.h"
#include "MIRGraph.h"
#include "IonLIR.h"
#include "jsscriptinlines.h"
#include "LinearScan.h"

using namespace js;
using namespace js::ion;

bool
C1Spewer::init(const char *path)
{
    spewout_ = fopen(path, "w");
    return (spewout_ != NULL);
}

void
C1Spewer::beginFunction(MIRGraph *graph, JSScript *script)
{
    if (!spewout_)
        return;

    this->graph  = graph;
    this->script = script;

    fprintf(spewout_, "begin_compilation\n");
    fprintf(spewout_, "  name \"%s:%d\"\n", script->filename, script->lineno);
    fprintf(spewout_, "  method \"%s:%d\"\n", script->filename, script->lineno);
    fprintf(spewout_, "  date %d\n", (int)time(NULL));
    fprintf(spewout_, "end_compilation\n");
}

void
C1Spewer::spewPass(const char *pass)
{
    if (!spewout_)
        return;

    fprintf(spewout_, "begin_cfg\n");
    fprintf(spewout_, "  name \"%s\"\n", pass);

    for (MBasicBlockIterator block(graph->begin()); block != graph->end(); block++)
        spewPass(spewout_, *block);

    fprintf(spewout_, "end_cfg\n");
    fflush(spewout_);
}

void
C1Spewer::spewIntervals(const char *pass, LinearScanAllocator *regalloc)
{
    if (!spewout_)
        return;

    fprintf(spewout_, "begin_intervals\n");
    fprintf(spewout_, " name \"%s\"\n", pass);

    size_t nextId = 0x4000;
    for (MBasicBlockIterator block(graph->begin()); block != graph->end(); block++)
        spewIntervals(spewout_, *block, regalloc, nextId);

    fprintf(spewout_, "end_intervals\n");
    fflush(spewout_);
}

void
C1Spewer::endFunction()
{
    return;
}

void
C1Spewer::finish()
{
    if (spewout_)
        fclose(spewout_);
}

static void
DumpDefinition(FILE *fp, MDefinition *def)
{
    fprintf(fp, "      ");
    fprintf(fp, "%u %lu ", def->id(), def->useCount());
    def->printName(fp);
    fprintf(fp, " ");
    def->printOpcode(fp);
    fprintf(fp, " <|@\n");
}

static void
DumpLIR(FILE *fp, LInstruction *ins)
{
    fprintf(fp, "      ");
    fprintf(fp, "%d ", ins->id());
    ins->print(fp);
    fprintf(fp, " <|@\n");
}

void
C1Spewer::spewIntervals(FILE *fp, MBasicBlock *block, LinearScanAllocator *regalloc, size_t &nextId)
{
    LBlock *lir = block->lir();

    for (LInstructionIterator ins = lir->begin(); ins != lir->end(); ins++) {
        for (size_t k = 0; k < ins->numDefs(); k++) {
            VirtualRegister *vreg = &regalloc->vregs[ins->getDef(k)->virtualRegister()];

            for (size_t i = 0; i < vreg->numIntervals(); i++) {
                LiveInterval *live = vreg->getInterval(i);
                if (live->numRanges()) {
                    fprintf(fp, "%d object \"", (i == 0) ? vreg->reg() : int32(nextId++));
                    LAllocation::PrintAllocation(fp, live->getAllocation());
                    fprintf(fp, "\" %d -1", vreg->reg());
                    for (size_t j = 0; j < live->numRanges(); j++) {
                        fprintf(fp, " [%d, %d[", live->getRange(j)->from.pos(),
                                live->getRange(j)->to.pos());
                    }
                    for (size_t j = 0; j < vreg->numUses(); j++)
                        fprintf(fp, " %d M", vreg->getUse(j)->ins->id() * 2);
                    fprintf(fp, " \"\"\n");
                }
            }
        }
    }
}
void
C1Spewer::spewPass(FILE *fp, MBasicBlock *block)
{
    fprintf(fp, "  begin_block\n");
    fprintf(fp, "    name \"B%d\"\n", block->id());
    fprintf(fp, "    from_bci -1\n");
    fprintf(fp, "    to_bci -1\n");

    fprintf(fp, "    predecessors");
    for (uint32 i = 0; i < block->numPredecessors(); i++) {
        MBasicBlock *pred = block->getPredecessor(i);
        fprintf(fp, " \"B%d\"", pred->id());
    }
    fprintf(fp, "\n");

    fprintf(fp, "    successors");
    for (uint32 i = 0; i < block->numSuccessors(); i++) {
        MBasicBlock *successor = block->getSuccessor(i);
        fprintf(fp, " \"B%d\"", successor->id());
    }
    fprintf(fp, "\n");

    fprintf(fp, "    xhandlers\n");
    fprintf(fp, "    flags\n");

    if (block->lir() && block->lir()->begin() != block->lir()->end()) {
        fprintf(fp, "    first_lir_id %d\n", block->lir()->firstId());
        fprintf(fp, "    last_lir_id %d\n", block->lir()->lastId());
    }

    fprintf(fp, "    begin_states\n");

    fprintf(fp, "      begin_locals\n");
    fprintf(fp, "        size %d\n", (int)block->numEntrySlots());
    fprintf(fp, "        method \"None\"\n");
    for (uint32 i = 0; i < block->numEntrySlots(); i++) {
        MDefinition *ins = block->getEntrySlot(i);
        fprintf(fp, "        ");
        fprintf(fp, "%d ", i);
        ins->printName(fp);
        fprintf(fp, "\n");
    }
    fprintf(fp, "      end_locals\n");

    fprintf(fp, "    end_states\n");

    fprintf(fp, "    begin_HIR\n");
    for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++)
        DumpDefinition(fp, *phi);
    for (MInstructionIterator i(block->begin()); i != block->end(); i++)
        DumpDefinition(fp, *i);
    fprintf(fp, "    end_HIR\n");

    if (block->lir()) {
        fprintf(fp, "    begin_LIR\n");
        for (size_t i = 0; i < block->lir()->numPhis(); i++)
            DumpLIR(fp, block->lir()->getPhi(i));
        for (LInstructionIterator i(block->lir()->begin()); i != block->lir()->end(); i++)
            DumpLIR(fp, *i);
        fprintf(fp, "    end_LIR\n");
    }

    fprintf(fp, "  end_block\n");
}

#endif /* DEBUG */

