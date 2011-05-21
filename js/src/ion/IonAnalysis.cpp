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

#include "IonBuilder.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonSpew.h"
#include "IonAnalysis.h"

using namespace js;
using namespace js::ion;

class TypeAnalyzer
{
    MIRGenerator *gen;
    MIRGraph &graph;
    js::Vector<MInstruction *, 0, ContextAllocPolicy> worklist;

  private:
    bool addToWorklist(MInstruction *ins);
    MInstruction *popFromWorklist();

  public:
    TypeAnalyzer(MIRGenerator *gen, MIRGraph &graph);

    bool analyze();
    bool inspectOperands(MInstruction *ins);
    bool reflow(MInstruction *ins);
};

TypeAnalyzer::TypeAnalyzer(MIRGenerator *gen, MIRGraph &graph)
  : gen(gen),
    graph(graph),
    worklist(gen->cx)
{
}

bool
TypeAnalyzer::addToWorklist(MInstruction *ins)
{
    if (!ins->inWorklist()){
        ins->setInWorklist();
        return worklist.append(ins);
    }
    return true;
}

MInstruction *
TypeAnalyzer::popFromWorklist()
{
    MInstruction *ins = worklist.popCopy();
    ins->setNotInWorklist();
    return ins;
}

bool
TypeAnalyzer::inspectOperands(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
    }
    return true;
}

bool
TypeAnalyzer::reflow(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
        if (!addToWorklist(ins->getOperand(i)->ins()))
            return false;
    }
    for (MUseIterator iter(ins); iter.more(); iter.next()) {
        if (!addToWorklist(iter->ins()))
            return false;
    }
    return true;
}

bool
TypeAnalyzer::analyze()
{
    // Populate the worklist.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        for (size_t i = 0; i < block->numPhis(); i++) {
            if (!addToWorklist(block->getPhi(i)))
                return false;
        }
        for (size_t i = 0; i < block->numInstructions(); i++) {
            if (!addToWorklist(block->getInstruction(i)))
                return false;
        }
    }

    while (!worklist.empty()) {
        MInstruction *ins = popFromWorklist();
        if (!inspectOperands(ins))
            return false;
    }

    return true;
}

bool
ion::ApplyTypeInformation(MIRGenerator *gen, MIRGraph &graph)
{
    TypeAnalyzer analysis(gen, graph);
    if (!analysis.analyze())
        return false;
    return true;
}

