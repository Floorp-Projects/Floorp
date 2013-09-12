// A breakpoint cleared during dispatch does not fire.
// (Breakpoint dispatch is well-behaved even when breakpoint handlers clear other breakpoints.)

var g = newGlobal();
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    var s = frame.script;
    function handler(i) {
        if (i === 1)
            return function () { log += i; s.clearBreakpoint(h[1]); s.clearBreakpoint(h[2]); };
        return function () { log += i; };
    }
    var offs = s.getLineOffsets(g.line0 + 2);
    var h = [];
    for (var i = 0; i < 4; i++) {
        h[i] = {hit: handler(i)};
        for (var j = 0; j < offs.length; j++)
            s.setBreakpoint(offs[j], h[i]);
    }
};

g.eval("var line0 = Error().lineNumber;\n" +
       "debugger;\n" +          // line0 + 1
       "result = 'ok';\n");     // line0 + 2
assertEq(log, '013');
