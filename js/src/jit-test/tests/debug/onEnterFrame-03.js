// frame.eval works in the enterFrame hook.
// It triggers the enterFrame hook again, recursively. (!)

var g = newGlobal();
g.a = ".";

var dbg = Debugger(g);
var nestCount = 0, N = 9;
var log = "";
dbg.onEnterFrame = function (frame) {
    assertEq(frame.type, "eval");
    if (nestCount < N) {
        log += '(';
        nestCount++;
        var a = frame.eval("a").return;
        log += a;
        nestCount--;
        log += ')';
    }
};

assertEq(g.eval("a"), ".");
assertEq(log, Array(N + 1).join("(") + Array(N + 1).join(".)"));
