// Frame.prototype.this in strict direct eval frames

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(frame.this, g.v);
};

g.eval("function f() { 'use strict'; eval('debugger;'); }");

g.eval("Boolean.prototype.f = f; v = true; v.f();");
g.eval("f.call(v);");
g.eval("v = null; f.call(v);");

assertEq(hits, 3);
