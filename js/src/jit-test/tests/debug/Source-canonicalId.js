// Test Debugger.Source.prototype.canonicalId

const g = newGlobal();

const dbg1 = new Debugger;
const dbg2 = new Debugger;

const gw1 = dbg1.addDebuggee(g);
const gw2 = dbg2.addDebuggee(g);

g.eval("function f(x) { return 2*x; }");
g.eval("function g(x) { return 2+x; }");

const fw1 = gw1.getOwnPropertyDescriptor('f').value;
const fw2 = gw2.getOwnPropertyDescriptor('f').value;
const hw1 = gw1.getOwnPropertyDescriptor('g').value;
const hw2 = gw2.getOwnPropertyDescriptor('g').value;

const fs1 = fw1.script.source;
const fs2 = fw2.script.source;
const gs1 = hw1.script.source;
const gs2 = hw2.script.source;

assertEq(!!fs1, true);
assertEq(!!fs2, true);
assertEq(fs1.canonicalId, fs2.canonicalId);

assertEq(!!gs1, true);
assertEq(!!gs2, true);
assertEq(gs1.canonicalId, gs2.canonicalId);

assertEq(fs1.canonicalId !== gs1.canonicalId, true);
