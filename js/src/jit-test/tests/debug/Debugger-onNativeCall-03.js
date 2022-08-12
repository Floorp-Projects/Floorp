// Test onNativeCall's behavior when used with self-hosted functions.

load(libdir + 'eqArrayHelper.js');

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

const rv = [];

dbg.onEnterFrame = f => {
  rv.push("EnterFrame");
};

dbg.onNativeCall = f => {
  rv.push(f.displayName);
};

gdbg.executeInGlobal(`
  var x = [1,3,2];
  x.sort((a, b) => {print(a)});
`);

assertEqArray(rv, [
  "EnterFrame", "sort",
  "ArraySortCompare/<",
  "EnterFrame", "print",
  "ArraySortCompare/<",
  "EnterFrame", "print",
]);
