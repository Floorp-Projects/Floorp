// clearBreakpoint clears breakpoints for the current Debugger object only.

var g = newGlobal('new-compartment');

var hits = 0;
var handler = {
    hit: function (frame) {
        hits++;
        frame.script.clearBreakpoint(handler);
    }
};

function attach(i) {
    var dbg = Debugger(g);
    dbg.onDebuggerStatement = function (frame) {
        var s = frame.script;
        var offs = s.getLineOffsets(g.line0 + 2);
        for (var i = 0; i < offs.length; i++)
            s.setBreakpoint(offs[i], handler);
    };
}
for (var i = 0; i < 4; i++)
    attach(i);

g.eval("var line0 = Error().lineNumber;\n" +
       "debugger;\n" +      // line0 + 1
       "Math.sin(0);\n");   // line0 + 2
assertEq(hits, 4);
