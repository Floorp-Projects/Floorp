// |jit-test| skip-if: !('gczeal' in this)

// This test case is a simplified version of debug/Source-invisible.js.

gczeal(2,21);

var gi = newGlobal();
gi.eval('function f() {}');

var gv = newGlobal();
gv.f = gi.f;
gv.eval('f = clone(f);');

var dbg = new Debugger;
