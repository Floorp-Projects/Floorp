// Test that the onNativeCall hook is called when native function is
// called inside self-hosted JS.

load(libdir + 'eqArrayHelper.js');

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger();
var gdbg = dbg.addDebuggee(g);

const rv = [];
dbg.onNativeCall = (callee, reason) => {
  rv.push(callee.name);
};

gdbg.executeInGlobal(`
// Built-in native function.
[1, 2, 3].map(Array.prototype.push, Array.prototype);

// Self-hosted function.
[1, 2, 3].map(String.prototype.padStart, "");

// Other native function.
[1, 2, 3].map(dateNow);

// Self-hosted function called internally.
"abc".match(/a./);
`);
assertEqArray(rv, [
  "map", "push", "push", "push",
  "map", "padStart", "padStart", "padStart",
  "map", "dateNow", "dateNow", "dateNow",
  "match", "[Symbol.match]",
]);
