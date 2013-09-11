// Handlers for breakpoints in an eval script are live as long as the script is on the stack.

var g = newGlobal();
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    function handler(i) {
        return {hit: function () { log += '' + i; }};
    }

    var s = frame.script;
    var offs = s.getLineOffsets(g.line0 + 2);
    for (var i = 0; i < 7; i++) {
        var h = handler(i);
        for (var j = 0; j < offs.length; j++)
            s.setBreakpoint(offs[j], h);
    }
    gc();
};


g.eval("var line0 = Error().lineNumber;\n" +
       "debugger;\n" +  // line0 + 1
       "x = 1;\n");     // line0 + 2
assertEq(log, '0123456');
