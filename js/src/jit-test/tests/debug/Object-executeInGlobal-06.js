// Debugger.Object.prototype.executeInGlobal sets 'this' to the global.

var dbg = new Debugger;
var g1 = newGlobal({newCompartment: true});
var g1w = dbg.addDebuggee(g1);

assertEq(g1w.executeInGlobal('this').return, g1w.makeDebuggeeValue(g1));
assertEq(g1w.executeInGlobalWithBindings('this', { x:42 }).return, g1w.makeDebuggeeValue(g1));
