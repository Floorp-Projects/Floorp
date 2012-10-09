// Debugger.Object.prototype.evalInGlobal sets 'this' to the global.

var dbg = new Debugger;
var g1 = newGlobal();
var g1w = dbg.addDebuggee(g1);

assertEq(g1w.evalInGlobal('this').return, g1w);
assertEq(g1w.evalInGlobalWithBindings('this', { x:42 }).return, g1w);
