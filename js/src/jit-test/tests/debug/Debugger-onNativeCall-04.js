// Test that onNativeCall behaves correctly when a debugger eval might enter the
// JIT via OSR.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

g.eval(`
const x = [];
function f() {
  for (let i = 0; i < 5; i++) {
    x.push(i);
  }
}
`);

let numCalls = 0;
dbg.onNativeCall = callee => { assertEq(callee.name, "push"); numCalls++; };

var dbg2 = Debugger(g);

for (let i = 0; i < 5; i++) {
  numCalls = 0;
  gdbg.executeInGlobal(`f()`);
  assertEq(numCalls, 5);
}
