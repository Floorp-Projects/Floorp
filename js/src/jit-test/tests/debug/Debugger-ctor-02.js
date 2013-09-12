// |jit-test| debug
// Test creating a Debugger in a sandbox, debugging the initial global.
//
// This requires debug mode to already be on in the initial global, since it's
// always on the stack in the shell. Hence the |jit-test| tag.

load(libdir + 'asserts.js');

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("var dbg = new Debugger(debuggeeGlobal);");
assertEq(g.eval("dbg instanceof Debugger"), true);

// The Debugger constructor from this compartment will not accept as its argument
// an Object from this compartment. Shenanigans won't fool the membrane.
g.parent = this;
assertThrowsInstanceOf(function () { g.eval("parent.Debugger(parent.Object())"); }, TypeError);
