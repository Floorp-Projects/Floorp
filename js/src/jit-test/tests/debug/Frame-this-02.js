// |jit-test| debug
// Frame.prototype.this in strict direct eval frames

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
var v;
dbg.hooks = {
    debuggerHandler: function (frame) {
        hits++;
        assertEq(frame.this, g.v);
    }
};

g.eval("function f() { 'use strict'; eval('debugger;'); }");

g.eval("Boolean.prototype.f = f; v = true; v.f();");
g.eval("f.call(v);");
g.eval("v = null; f.call(v);");

assertEq(hits, 3);
