// The same object gets the same Debugger.Object wrapper at different times, if the difference would be observable.

var N = HOTLOOP + 4;

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var wrappers = [];

dbg.onDebuggerStatement = function (frame) { wrappers.push(frame.arguments[0]); };
g.eval("var originals = []; function f(x) { originals.push(x); debugger; }");
for (var i = 0; i < N; i++)
    g.eval("f({});");
assertEq(wrappers.length, N);

for (var i = 0; i < N; i++)
    for (var j = i + 1; j < N; j++)
        assertEq(wrappers[i] === wrappers[j], false);

gc();

dbg.onDebuggerStatement = function (frame) { assertEq(frame.arguments[0], wrappers.pop()); };
g.eval("function h(x) { debugger; }");
for (var i = 0; i < N; i++)
    g.eval("h(originals.pop());");
assertEq(wrappers.length, 0);
