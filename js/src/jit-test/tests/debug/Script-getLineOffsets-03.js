// getLineOffsets treats one-line compound statements as having only one entry-point.
// (A breakpoint on a line that only executes once will only hit once.)

var g = newGlobal();
g.line0 = null;
var dbg = Debugger(g);
var log;
dbg.onDebuggerStatement = function (frame) {
    var s = frame.script;
    var lineno = g.line0 + 2;
    var offs = s.getLineOffsets(lineno);
    for (var i = 0; i < offs.length; i++) {
        assertEq(s.getOffsetLocation(offs[i]).lineNumber, lineno);
        s.setBreakpoint(offs[i], {hit: function () { log += 'B'; }});
    }
    log += 'A';
};

function test(s) {
    log = '';
    g.eval("line0 = Error().lineNumber;\n" +
           "debugger;\n" +  // line0 + 1
           s);              // line0 + 2
    assertEq(log, 'AB');
}

g.i = 0;
g.j = 0;
test("{i++; i--; i++; i--; }");
test("if (i === 0) i++; else i--;");
test("while (i < 5) i++;");
test("do --i; while (i > 0);");
test("for (i = 0; i < 5; i++) j++;");
test("for (var x in [0, 1, 2]) j++;");
test("switch (i) { case 0: j = 0; case 1: j = 1; case 2: j = 2; default: j = i; }");
test("switch (i) { case 'A': j = 0; case 'B': j = 1; case 'C': j = 2; default: j = i; }");
