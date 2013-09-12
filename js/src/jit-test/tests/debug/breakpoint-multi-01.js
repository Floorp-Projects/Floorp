// A single Debugger object can set multiple breakpoints at an instruction.

var g = newGlobal();
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    log += 'D';
    function handler(i) {
        return {hit: function (frame) { log += '' + i; }};
    }
    var f = frame.eval("f").return;
    var s = f.script;
    var offs = s.getLineOffsets(g.line0 + 2);
    for (var i = 0; i < 10; i++) {
        var bp = handler(i);
        for (var j = 0; j < offs.length; j++)
            s.setBreakpoint(offs[j], bp);
    }
    assertEq(f.call().return, 42);
    log += 'X';
};

g.eval("var line0 = Error().lineNumber;\n" +
       "function f() {\n" +  // line0 + 1
       "    return 42;\n" +  // line0 + 2
       "}\n" +
       "debugger;\n");
assertEq(log, 'D0123456789X');
