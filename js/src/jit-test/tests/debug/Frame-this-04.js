// Debugger.Frame.prototype.this in functions, with object values

function classOf(obj) {
    return Object.prototype.toString.call(obj).match(/^\[object (.*)\]$/)[1];
}

var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(frame.this instanceof Debugger.Object, true);
    assertEq(frame.this.class, classOf(Object(g.v)));
};

g.eval("function f() { debugger; }");

g.eval("v = {}; f.call(v);");
g.eval("v.f = f; v.f();");
g.eval("v = new Date; f.call(v);");
g.eval("v.f = f; v.f();");
g.eval("v = []; f.call(v);");
g.eval("Object.prototype.f = f; v.f();");
g.eval("v = this; f();");
assertEq(hits, 7);
