/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineFrame.h"

using namespace js;
using namespace js::ion;

void
BaselineFrame::trace(JSTracer *trc)
{
    gc::MarkObjectRoot(trc, &scopeChain_, "baseline-scopechain");

    // Mark locals and stack values.
    size_t nvalues = numValueSlots();
    if (nvalues > 0) {
        // The stack grows down, so start at the last Value.
        Value *last = valueSlot(nvalues - 1);
        gc::MarkValueRootRange(trc, nvalues, last, "baseline-stack");
    }
}
