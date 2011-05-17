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

#include "BytecodeAnalyzer.h"
#include "Ion.h"
#include "IonSpew.h"
#include "MIRGraph.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

C1Spewer::C1Spewer(MIRGraph &graph, JSScript *script)
  : graph(graph),
    script(script),
    spewout_(NULL)
{
}

C1Spewer::~C1Spewer()
{
    fclose(spewout_);
}

void
C1Spewer::enable(const char *path)
{
    spewout_ = fopen(path, "w");
    if (!spewout_)
        return;

    fprintf(spewout_, "begin_compilation\n");
    fprintf(spewout_, "  name \"%s:%d\"\n", script->filename, script->lineno);
    fprintf(spewout_, "  method \"%s:%d\"\n", script->filename, script->lineno);
    fprintf(spewout_, "  date %d\n", (int)time(NULL));
    fprintf(spewout_, "end_compilation\n");
}

void
C1Spewer::spew(const char *pass)
{
    if (spewout_)
        spew(spewout_, pass);
}

void
C1Spewer::spew(FILE *fp, const char *pass)
{
    fprintf(fp, "begin_cfg\n");
    fprintf(fp, "  name \"%s\"\n", pass);

    for (size_t i = 0; i < graph.numBlocks(); i++)
        spew(fp, graph.getBlock(i));

    fprintf(fp, "end_cfg\n");
}

static void
DumpInstruction(FILE *fp, MInstruction *ins)
{
    fprintf(fp, "      ");
    fprintf(fp, "0 %d ", ins->useCount());
    ins->printName(fp);
    fprintf(fp, " ");
    ins->printName(fp);
    fprintf(fp, " ");
    for (size_t j = 0; j < ins->numOperands(); j++) {
        ins->getOperand(j)->printName(fp);
        if (j != ins->numOperands() - 1)
            fprintf(fp, ", ");
    }
    fprintf(fp, " <|@\n");
}

void
C1Spewer::spew(FILE *fp, MBasicBlock *block)
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
    MControlInstruction *last = block->lastIns();
    for (uint32 i = 0; i < last->numSuccessors(); i++) {
        MBasicBlock *successor = last->getSuccessor(i);
        fprintf(fp, " \"B%d\"", successor->id());
    }
    fprintf(fp, "\n");

    fprintf(fp, "    xhandlers\n");
    fprintf(fp, "    flags\n");

    fprintf(fp, "    begin_states\n");

    fprintf(fp, "      begin_locals\n");
    fprintf(fp, "        size %d\n", block->numEntrySlots());
    fprintf(fp, "        method \"None\"\n");
    for (uint32 i = 0; i < block->numEntrySlots(); i++) {
        MInstruction *ins = block->getEntrySlot(i);
        fprintf(fp, "        ");
        fprintf(fp, "%d ", i);
        ins->printName(fp);
        fprintf(fp, "\n");
    }
    fprintf(fp, "      end_locals\n");

    fprintf(fp, "    end_states\n");

    fprintf(fp, "    begin_HIR\n");
    for (size_t i = 0; i < block->numPhis(); i++)
        DumpInstruction(fp, block->getPhi(i));
    for (size_t i = 0; i < block->numInstructions(); i++)
        DumpInstruction(fp, block->getInstruction(i));
    fprintf(fp, "    end_HIR\n");

    fprintf(fp, "  end_block\n");
}

