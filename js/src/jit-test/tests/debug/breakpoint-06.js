// The argument to a breakpoint hit method is a frame.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame1) {
    function hit(frame2) {
        assertEq(frame2, frame1);
        hits++;
    }
    var s = frame1.script;
    var offs = s.getLineOffsets(g.line0 + 2);
    for (var i = 0; i < offs.length; i++)
        s.setBreakpoint(offs[i], {hit: hit});
};
g.eval("var line0 = Error().lineNumber;\n" +
       "debugger;\n" +  // line0 + 1
       "x = 1;\n");     // line0 + 2
assertEq(hits, 1);
assertEq(g.x, 1);
