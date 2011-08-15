// getLineOffsets identifies multiple ways to land on a line.

var g = newGlobal('new-compartment');
g.line0 = null;
var dbg = Debugger(g);
var where;
dbg.onDebuggerStatement = function (frame) {
    var s = frame.script, lineno, offs;

    lineno = g.line0 + where;
    offs = s.getLineOffsets(lineno);
    for (var i = 0; i < offs.length; i++) {
        assertEq(s.getOffsetLine(offs[i]), lineno);
        s.setBreakpoint(offs[i], {hit: function () { g.log += 'B'; }});
    }

    lineno++;
    offs = s.getLineOffsets(lineno);
    for (var i = 0; i < offs.length; i++) {
        assertEq(s.getOffsetLine(offs[i]), lineno);
        s.setBreakpoint(offs[i], {hit: function () { g.log += 'C'; }});
    }

    g.log += 'A';
};

function test(s) {
    assertEq(s.charAt(s.length - 1), '\n');
    var count = (s.split(/\n/).length - 1); // number of lines in s
    g.log = '';
    where = 1 + count;
    g.eval("line0 = Error().lineNumber;\n" +
           "debugger;\n" +          // line0 + 1
           s +                      // line0 + 2 ... line0 + where
           "log += 'D';\n");
    assertEq(g.log, 'AB!CD');
}

// if-statement with yes and no paths on a single line
g.i = 0;
test("if (i === 0)\n" +
     "    log += '!'; else log += 'X';\n");
test("if (i === 2)\n" +
     "    log += 'X'; else log += '!';\n");

// break to a line that has code inside and outside the loop
g.i = 2;
test("while (1) {\n" +
     "    if (i === 2) break;\n" +
     "    log += 'X'; } log += '!';\n");

// leaving a while loop by failing the test, when the last line has stuff both inside and outside the loop
g.i = 0;
test("while (i > 0) {\n" +
     "    if (i === 70) log += 'X';\n" +
     "    --i;  } log += '!';\n");

// multiple case-labels on the same line
g.i = 0;
test("switch (i) {\n" +
     "    case 0: case 1: log += '!'; break; }\n");
test("switch ('' + i) {\n" +
     "    case '0': case '1': log += '!'; break; }\n");
test("switch (i) {\n" +
     "    case 'ok' + i: case i - i: log += '!'; break; }\n");
