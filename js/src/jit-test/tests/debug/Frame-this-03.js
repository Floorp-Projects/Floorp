// |jit-test| debug
// Frame.prototype.this in non-strict-mode functions, with primitive values

function classOf(obj) {
    return Object.prototype.toString.call(obj).match(/^\[object (.*)\]$/)[1];
}

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        hits++;
        assertEq(typeof frame.this, 'object');
        assertEq(frame.this.getClass(), g.v == null ? classOf(g) : classOf(Object(g.v)));
    }
};

g.eval("function f() { debugger; }");

g.eval("Boolean.prototype.f = f; v = true; v.f();");
g.eval("f.call(v);");
g.eval("Number.prototype.f = f; v = 3.14; v.f();");
g.eval("f.call(v);");
g.eval("String.prototype.f = f; v = 'hello'; v.f();");
g.eval("f.call(v);");
g.eval("v = undefined; f.call(v);");
g.eval("v = null; f.call(v);");

assertEq(hits, 8);
