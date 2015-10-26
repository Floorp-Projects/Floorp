// Basic breakpoint test.

var g = newGlobal();
g.s = '';
var handler = {
    hit: function (frame) {
        assertEq(this, handler);
        g.s += '1';
    }
};
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    g.s += '0';
    var line0 = frame.script.getOffsetLocation(frame.offset).lineNumber;
    var offs = frame.script.getLineOffsets(line0 + 2);
    for (var i = 0; i < offs.length; i++)
        frame.script.setBreakpoint(offs[i], handler);
};
g.eval("debugger;\n" +
       "s += 'a';\n" +  // line0 + 1
       "s += 'b';\n");  // line0 + 2
assertEq(g.s, "0a1b");
