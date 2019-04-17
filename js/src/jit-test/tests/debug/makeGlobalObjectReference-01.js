// Debugger.prototype.makeGlobalObjectReference returns a D.O for a global
// without adding it as a debuggee.

let g1 = newGlobal({newCompartment: true});
let dbg = new Debugger;
assertEq(dbg.hasDebuggee(g1), false);

let g1w = dbg.makeGlobalObjectReference(g1);
assertEq(dbg.hasDebuggee(g1), false);
assertEq(g1w.unsafeDereference(), g1);
assertEq(g1, g1w.makeDebuggeeValue(g1).unsafeDereference());

assertEq(dbg.addDebuggee(g1w), g1w);
assertEq(dbg.hasDebuggee(g1), true);
assertEq(dbg.hasDebuggee(g1w), true);
assertEq(g1w.unsafeDereference(), g1);
assertEq(g1, g1w.makeDebuggeeValue(g1).unsafeDereference());

// makeGlobalObjectReference dereferences CCWs.
let g2 = newGlobal({newCompartment: true});
g2.g1 = g1;
let g2w = dbg.addDebuggee(g2);
let g2g1w = g2w.getOwnPropertyDescriptor('g1').value;
assertEq(g2g1w !== g1w, true);
assertEq(g2g1w.unwrap(), g1w.makeDebuggeeValue(g1));
assertEq(dbg.makeGlobalObjectReference(g2g1w), g1w);
