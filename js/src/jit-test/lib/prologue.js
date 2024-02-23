/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var appendToActual = function(s) {
    actual += s + ',';
}

// Add dummy versions of missing functions and record whether they were
// originally present.
//
// This only affects the main global. Any globals created by the test will
// lack the function.
let hasFunction = {};
for (const name of [
    // Functions present if JS_GC_ZEAL defined:
    "gczeal",
    "unsetgczeal",
    "schedulegc",
    "selectforgc",
    "verifyprebarriers",
    "verifypostbarriers",
    "currentgc",
    "deterministicgc",
    "dumpGCArenaInfo",
    "setMarkStackLimit",
    // Functions present if DEBUG or JS_OOM_BREAKPOINT defined:
    "oomThreadTypes",
    "oomAfterAllocations",
    "oomAtAllocation",
    "resetOOMFailure",
    "oomTest",
    "stackTest",
    "interruptTest"]) {
    const present = name in this;
    if (!present) {
        this[name] = function() {};
    }
    hasFunction[name] = present;
}

// Set the minimum heap size for parallel marking to zero for testing purposes.
gcparam('parallelMarkingThresholdMB', 0);
