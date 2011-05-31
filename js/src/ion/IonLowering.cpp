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
#include "IonLowering.h"

using namespace js;
using namespace js::ion;

// The lowering phase looks at each opcode and decides its result and input
// types.
class LoweringPhase : public MInstructionVisitor
{
    MIRGraph &graph;

  public:
    LoweringPhase(MIRGraph &graph)
      : graph(graph)
    { }

    bool lowerBlock(MBasicBlock *block);
    bool lowerInstruction(MInstruction *ins);
    bool analyze();

#define VISITOR(op) bool visit(M##op *ins) { return true; }
    MIR_OPCODE_LIST(VISITOR)
#undef VISITOR
};

bool
LoweringPhase::lowerInstruction(MInstruction *ins)
{
    return ins->accept(this);
}

bool
LoweringPhase::lowerBlock(MBasicBlock *block)
{
    // Analyze phis first.
    for (size_t i = 0; i < block->numPhis(); i++) {
        if (!lowerInstruction(block->getPhi(i)))
            return false;
    }
    for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
        if (!lowerInstruction(*i))
            return false;
    }
    return true;
}

bool
LoweringPhase::analyze()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        if (!lowerBlock(graph.getBlock(i)))
            return false;
    }
    return true;
}

bool
ion::Lower(MIRGraph &graph)
{
    LoweringPhase phase(graph);
    return phase.analyze();
}

