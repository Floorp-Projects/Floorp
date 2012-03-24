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
RangeAnalysis::analyze()
{
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MDefinitionIterator iter(*block); iter; iter++) {
            (*iter)->analyzeRange();
        }
    }

    return true;
}
