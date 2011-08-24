// |jit-test| debug
// The GC can cope with old and new breakpoints at the same position.

// This is a regression test for a bug Bill McCloskey found just by looking at
// the source code. See bug 677386 comment 8. Here we're testing that the trap
// string is correctly marked. (The silly expression for the trap string is to
// ensure that it isn't constant-folded; it's harder to get a compile-time
// constant to be GC'd.)

var g = newGlobal('new-compartment');
g.eval("var d = 0;\n" +
       "function f() { return 'ok'; }\n" +
       "trap(f, 0, Array(17).join('\\n') + 'd++;');\n");

var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var fw = gw.getOwnPropertyDescriptor("f").value;
var bp = {hits: 0, hit: function (frame) { this.hits++; }};
fw.script.setBreakpoint(0, bp);

gc();

g.f();
assertEq(g.d, 1);
assertEq(bp.hits, 1);
