var g = newGlobal();
var dbg = new Debugger(g);

var hits = 0;
dbg.onDebuggerStatement = function(frame) {
    ++hits;
    frame.older.eval("escaped = function() { return y }");
}

var arr = [];
const N = 10;

for (var i = 0; i < N; ++i) {
    g.escaped = undefined;
    g.eval("function h() { debugger }");
    g.eval("(function () { var y = {p:42}; h(); yield })().next();");
    assertEq(g.eval("escaped().p"), 42);
    arr.push(g.escaped);
}

gc();

for (var i = 0; i < N; ++i)
    arr[i]();
