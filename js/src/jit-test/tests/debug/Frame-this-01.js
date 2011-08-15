// Frame.prototype.this in strict-mode functions, with primitive values

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(frame.this, g.v);
};

g.eval("function f() { 'use strict'; debugger; }");

g.eval("Boolean.prototype.f = f; v = true; v.f();");
g.eval("f.call(v);");
g.eval("Number.prototype.f = f; v = 3.14; v.f();");
g.eval("f.call(v);");
g.eval("String.prototype.f = f; v = 'hello'; v.f();");
g.eval("f.call(v);");
g.eval("v = undefined; f.call(v);");
g.eval("v = null; f.call(v);");

assertEq(hits, 8);
