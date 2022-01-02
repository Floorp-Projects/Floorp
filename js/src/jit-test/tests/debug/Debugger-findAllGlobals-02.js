// Debugger.prototype.findAllGlobals finds ALL the globals!

var g1 = newGlobal({newCompartment: true});           // Created before the Debugger; debuggee.
var g2 = newGlobal({newCompartment: true});           // Created before the Debugger; not debuggee.

var dbg = new Debugger;

var g3 = newGlobal({newCompartment: true});           // Created after the Debugger; debuggee.
var g4 = newGlobal({newCompartment: true});           // Created after the Debugger; not debuggee.

var g1w = dbg.addDebuggee(g1);
var g3w = dbg.addDebuggee(g3);

var a = dbg.findAllGlobals();

// Get Debugger.Objects around global objects
var g2w = dbg.makeGlobalObjectReference(g2);
var g4w = dbg.makeGlobalObjectReference(g4);
var thisw = dbg.makeGlobalObjectReference(this);

// Check that they're all there.
assertEq(a.indexOf(g1w) != -1, true);
assertEq(a.indexOf(g2w) != -1, true);
assertEq(a.indexOf(g3w) != -1, true);
assertEq(a.indexOf(g4w) != -1, true);
assertEq(a.indexOf(thisw) != -1, true);
