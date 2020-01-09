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

// When running self-hosted code, we will see native calls to internal
// self-hosted JS functions and intrinsic natives. Drop these from the result
// array.
const validNames = ["EnterFrame", "sort", "print"];
const filtered = rv.filter(name => validNames.includes(name));

assertEq(filtered.length < rv.length, true);
assertEqArray(filtered, ["EnterFrame", "sort", "EnterFrame", "print", "EnterFrame", "print"]);
