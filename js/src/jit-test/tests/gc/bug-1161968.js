// |jit-test| skip-if: !('gczeal' in this)

// This test case is a simplified version of debug/Source-invisible.js.

gczeal(2,21);

var gi = newGlobal();

var gv = newGlobal();
gi.evaluate('function f() {}', {global: gv});

var dbg = new Debugger;
