// Test that the onNativeCall hook is called when native function is
// called via DebuggerObject.apply DebuggerObject.call

load(libdir + "eqArrayHelper.js");

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger();
var gdbg = dbg.addDebuggee(g);

g.eval(`
function f() {
  Array.from([1, 2]);
}
`);
const fdbgObj = gdbg.getOwnPropertyDescriptor("f").value;

let rv = [];
dbg.onNativeCall = (callee, reason) => {
  rv.push(callee.name);
};

fdbgObj.call();
assertEqArray(rv, ["from", "values", "next", "next", "next"]);

rv = [];
fdbgObj.apply();
assertEqArray(rv, ["from", "values", "next", "next", "next"]);
