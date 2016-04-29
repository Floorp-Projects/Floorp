/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AliasAnalysisShared.h"

namespace js {
namespace jit {

void
AliasAnalysisShared::spewDependencyList()
{
#ifdef JS_JITSPEW
    if (JitSpewEnabled(JitSpew_AliasSummaries)) {
        Fprinter &print = JitSpewPrinter();
        JitSpewHeader(JitSpew_AliasSummaries);
        print.printf("Dependency list for other passes:\n");

        for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
            for (MInstructionIterator def(block->begin()), end(block->begin(block->lastIns()));
                 def != end;
                 ++def)
            {
                if (!def->dependency())
                    continue;
                if (!def->getAliasSet().isLoad())
                    continue;

                JitSpewHeader(JitSpew_AliasSummaries);
                print.printf(" ");
                MDefinition::PrintOpcodeName(print, def->op());
                print.printf("%d marked depending on ", def->id());
                MDefinition::PrintOpcodeName(print, def->dependency()->op());
                print.printf("%d\n", def->dependency()->id());
            }
        }
    }
#endif
}

} // namespace jit
} // namespace js
