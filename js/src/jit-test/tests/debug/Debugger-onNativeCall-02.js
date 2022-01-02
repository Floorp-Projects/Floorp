// Test that the onNativeCall hook can control the call's behavior.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

g.eval(`
var x = [];
Object.defineProperty(x, "a", {
  get: print,
  set: print,
});
var rv;
function f() {
  x.a++;
  try {
    rv = x.push(4);
  } catch (e) {
    throw "rethrowing";
  }
}
`);

for (let i = 0; i < 5; i++) {
  g.f();
}

for (let i = 0; i < 5; i++) {
  // Test terminating execution.
  dbg.onNativeCall = (callee, reason) => {
    return null;
  };
  const len = g.x.length;
  let v = gdbg.executeInGlobal(`f()`);
  assertEq(v, null);
  assertEq(g.x.length, len);

  // Test throwing an exception.
  dbg.onNativeCall = (callee, reason) => {
    return { throw: "throwing" };
  };
  v = gdbg.executeInGlobal(`f()`);
  assertEq(v.throw, "throwing");

  // Test throwing an exception #2.
  dbg.onNativeCall = (callee, reason) => {
    if (callee.name == "push") {
      return { throw: "throwing" };
    }
  };
  v = gdbg.executeInGlobal(`f()`);
  assertEq(v.throw, "rethrowing");

  // Test returning a different value from the native.
  dbg.onNativeCall = (callee, reason) => {
    return { return: "value" };
  };
  v = gdbg.executeInGlobal(`f()`);
  assertEq(v.return, undefined);
  assertEq(g.rv, "value");
}
