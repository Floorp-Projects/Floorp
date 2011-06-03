// |jit-test| debug
// Test creating a Debug in a sandbox, debugging the initial global.
//
// This requires debug mode to already be on in the initial global, since it's
// always on the stack in the shell. Hence the |jit-test| tag.

load(libdir + 'asserts.js');

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var dbg = new Debug(debuggeeGlobal);");
assertEq(g.eval("dbg instanceof Debug"), true);

// The Debug constructor from this compartment will not accept as its argument
// an Object from this compartment. Shenanigans won't fool the membrane.
g.parent = this;
assertThrowsInstanceOf(function () { g.eval("parent.Debug(parent.Object())"); }, TypeError);
