// Frame.prototype.this in non-strict-mode functions, with primitive values

function classOf(obj) {
    return Object.prototype.toString.call(obj).match(/^\[object (.*)\]$/)[1];
}

var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(frame.this instanceof Debugger.Object, true);
    assertEq(frame.this.class, g.v == null ? classOf(g) : classOf(Object(g.v)));
};

g.eval("function f() { debugger; }");

g.eval("Boolean.prototype.f = f; v = true; v.f();");
g.eval("f.call(v);");
g.eval("Number.prototype.f = f; v = 3.14; v.f();");
g.eval("f.call(v);");
g.eval("String.prototype.f = f; v = 'hello'; v.f();");
g.eval("f.call(v);");
g.eval("Symbol.prototype.f = f; v = Symbol('world'); v.f();");
g.eval("f.call(v);");
g.eval("v = undefined; f.call(v);");
g.eval("v = null; f.call(v);");

assertEq(hits, 10);
