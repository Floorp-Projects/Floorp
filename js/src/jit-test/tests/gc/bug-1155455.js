// |jit-test| error: TypeError; skip-if: !('gczeal' in this)
var g = newGlobal();
gczeal(10, 2)
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame1) {
    function hit(frame2) {
      return hit[0] = "mutated";
    }
    var s = frame1.script;
    var offs = s.getLineOffsets(g.line0 + 2);
    for (var i = 0; i < offs.length; i++)
      s.setBreakpoint(offs[i], {hit: hit});
    return;
};
var lfGlobal = newGlobal();
g.eval("var line0 = Error().lineNumber;\n debugger;\nx = 1;\n");
