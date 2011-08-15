// Storing a Debugger.Object as a key in a WeakMap protects it from GC as long as
// the referent is alive.

var g = newGlobal('new-compartment');
var N = g.N = 10;
var dbg = Debugger(g);
var cache = new WeakMap;

var i = 0;
dbg.onDebuggerStatement = function (frame) {
    cache.set(frame.arguments[0], i++);
};
g.eval("function f(x) { debugger; }");
g.eval("var arr = [], j; for (j = 0; j < N; j++) arr[j] = {};");
g.eval("for (j = 0; j < N; j++) f(arr[j]);");
assertEq(i, N);

gc(); gc();

i = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(cache.get(frame.arguments[0]), i++)
};
g.eval("for (j = 0; j < N; j++) f(arr[j]);");
assertEq(i, N);
