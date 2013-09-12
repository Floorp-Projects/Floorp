// The .name of a non-function object is undefined.

var g = newGlobal();
var hits = 0;
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.arguments[0].name, undefined);
    hits++;
};
g.eval("function f(nonfunction) { debugger; }");

g.eval("f({});");
g.eval("f(/a*/);");
g.eval("f({name: 'bad'});");
g.eval("f(Proxy.createFunction({name: {value: 'bad'}}, function () {}));");
assertEq(hits, 4);
