// Test that the onNativeCall hook is called when expected.

load(libdir + 'eqArrayHelper.js');

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

assertEq(gdbg.getProperty("print").return.isSameNative(print), true);
assertEq(gdbg.getProperty("print").return.isSameNative(newGlobal), false);

g.eval(`
const x = [];
Object.defineProperty(x, "a", {
  get: print,
  set: print,
});
function f() {
  x.a++;
  x.length = 0;
  x.push(4, 5, 6);
  x.sort(print);
}
`);

const comparisons = [
  print,
  Array.prototype.push,
  Array.prototype.sort, // Note: self-hosted
  newGlobal
];

const rv = [];
dbg.onNativeCall = (callee, reason) => {
  for (const fn of comparisons) {
    if (callee.isSameNative(fn)) {
      rv.push(fn.name);
    }
  }
}

for (let i = 0; i < 5; i++) {
  rv.length = 0;
  gdbg.executeInGlobal(`f()`);
  assertEqArray(rv, [
    "print", "print", "push",
    "sort", "print", "print",
  ]);
}
