// Basic Script.prototype.getBreakpoints tests.

var g = newGlobal();
g.eval("var line0 = Error().lineNumber;\n" +
       "function f(x) {\n" +        // line0 + 1
       "    if (x < 0)\n" +         // line0 + 2
       "        return -x;\n" +     // line0 + 3
       "    return x;\n" +
       "}");

var s;
var offsets = [];
var handlers = [];
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    s = frame.eval("f").return.script;
    var off;

    for (var i = 0; i < 3; i++) {
        var off = s.getLineOffsets(g.line0 + 2 + i)[0];
        assertEq(typeof off, 'number');
        handlers[i] = {};
        s.setBreakpoint(off, handlers[i]);
        offsets[i] = off;
    }
};
g.eval("debugger;");

// getBreakpoints without an offset gets all breakpoints in the script.
var bps = s.getBreakpoints();
assertEq(bps.length, handlers.length);
for (var i = 0; i < bps.length; i++)
    assertEq(bps.indexOf(handlers[i]) !== -1, true);

// getBreakpoints with an offset finds only breakpoints at that offset.
for (var i = 0; i < offsets.length; i++) {
    var bps = s.getBreakpoints(offsets[i]);
    assertEq(bps.length, 1);
    assertEq(bps[0], handlers[i]);
}
