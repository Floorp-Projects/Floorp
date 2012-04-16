/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "Ion.h"
#include "IonSpewer.h"
#include "RangeAnalysis.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace js::ion;

RangeAnalysis::RangeAnalysis(MIRGraph &graph)
  : graph(graph)
{
}

bool
RangeAnalysis::analyzeLate()
{
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MDefinitionIterator iter(*block); iter; iter++)
            iter->analyzeRangeForward();
    }

    for (PostorderIterator block(graph.poBegin()); block != graph.poEnd(); block++) {
        for (MInstructionReverseIterator riter(block->rbegin()); riter != block->rend(); riter++)
            riter->analyzeRangeBackward();
    }

    return true;
}

bool
RangeAnalysis::analyzeEarly()
{

    for (PostorderIterator block(graph.poBegin()); block != graph.poEnd(); block++) {
        for (MInstructionReverseIterator riter(block->rbegin()); riter != block->rend(); riter++)
            riter->analyzeTruncateBackward();
    }

    return true;
}

bool
RangeAnalysis::AllUsesTruncate(MInstruction *m)
{
    for (MUseIterator use = m->usesBegin(); use != m->usesEnd(); use++) {
        if (use->node()->isResumePoint())
            return false;

        MDefinition *def = use->node()->toDefinition();
        if (def->isTruncateToInt32())
            continue;
        if (def->isBitAnd())
            continue;
        if (def->isBitOr())
            continue;
        if (def->isBitXor())
            continue;
        if (def->isLsh())
            continue;
        if (def->isRsh())
            continue;
        if (def->isBitNot())
            continue;
        if (def->isAdd() && def->toAdd()->isTruncated())
            continue;
        if (def->isSub() && def->toSub()->isTruncated())
            continue;
        // cannot use divide, since |truncate(int32(x/y) + int32(a/b)) != truncate(x/y+a/b)|
        return false;
    }
    return true;
}
