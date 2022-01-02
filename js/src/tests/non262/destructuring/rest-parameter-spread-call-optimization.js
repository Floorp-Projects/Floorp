/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure the spread-call optimization doesn't break when a destructuring rest
// parameter is used.

function spreadTarget() { return arguments.length; }

function spreadOpt(...[r]){ return spreadTarget(...r); }
assertEq(spreadOpt([]), 0);
assertEq(spreadOpt([10]), 1);
assertEq(spreadOpt([10, 20]), 2);
assertEq(spreadOpt([10, 20, 30]), 3);

function spreadOpt2(...[...r]){ return spreadTarget(...r); }
assertEq(spreadOpt2(), 0);
assertEq(spreadOpt2(10), 1);
assertEq(spreadOpt2(10, 20), 2);
assertEq(spreadOpt2(10, 20, 30), 3);

function spreadOpt3(r, ...[]){ return spreadTarget(...r); }
assertEq(spreadOpt3([]), 0);
assertEq(spreadOpt3([10]), 1);
assertEq(spreadOpt3([10, 20]), 2);
assertEq(spreadOpt3([10, 20, 30]), 3);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
