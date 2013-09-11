// test frame.eval in non-top frames

var g = newGlobal();
var N = g.N = 12; // must be even
assertEq(N % 2, 0);
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var n = frame.eval("n").return;
    if (n === 0) {
        for (var i = 0; i <= N; i++) {
            assertEq(frame.type, 'call');
            assertEq(frame.callee.name, i % 2 === 0 ? 'even' : 'odd');
            assertEq(frame.eval("n").return, i);
            frame = frame.older;
        }
        assertEq(frame.type, 'call');
        assertEq(frame.callee.name, undefined);
        frame = frame.older;
        assertEq(frame.type, 'eval');
        hits++;
    }
};

var result = g.eval("(" + function () {
        function odd(n) { return n > 0 && !even(n - 1); }
        function even(n) { debugger; return n == 0 || !odd(n - 1); }
        return even(N);
    } + ")();");
assertEq(result, true);
assertEq(hits, 1);
