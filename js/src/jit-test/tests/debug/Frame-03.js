// When the debugger is triggered from different stack frames that happen to
// occupy the same memory, it delivers different Debugger.Frame objects.

var g = newGlobal();
var dbg = Debugger(g);
var hits;
var a = [];
dbg.onDebuggerStatement = function (frame) {
    for (var i = 0; i < a.length; i++) 
        assertEq(a[i] === frame, false);
    a.push(frame);
    hits++;
};

g.eval("function f() { debugger; }");
g.eval("function h() { debugger; f(); }");
hits = 0;
g.eval("for (var i = 0; i < 4; i++) h();");
assertEq(hits, 8);
