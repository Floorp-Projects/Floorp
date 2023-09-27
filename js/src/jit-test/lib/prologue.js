/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var appendToActual = function(s) {
    actual += s + ',';
}

// Add dummy versions of missing functions and record whether they
// were originally present.
let hasFunction = {};
for (const name of ["gczeal",
                    "schedulegc",
                    "gcslice",
                    "selectforgc",
                    "verifyprebarriers",
                    "verifypostbarriers",
                    "gcPreserveCode",
                    "setMarkStackLimit"]) {
    const present = name in this;
    if (!present) {
        this[name] = function() {};
    }
    hasFunction[name] = present;
}

// Set the minimum heap size for parallel marking to zero for testing purposes.
gcparam('parallelMarkingThresholdMB', 0);
