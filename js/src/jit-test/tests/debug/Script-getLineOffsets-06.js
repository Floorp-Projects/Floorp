// getLineOffsets works with extended instructions, such as JSOP_GOTOX.

var g = newGlobal();
g.line0 = null;
var dbg = Debugger(g);
var where;
dbg.onDebuggerStatement = function (frame) {
    var s = frame.script;
    var offs;
    var lineno = g.line0 + where;
    offs = s.getLineOffsets(lineno);
    for (var i = 0; i < offs.length; i++) {
        assertEq(s.getOffsetLocation(offs[i]).lineNumber, lineno);
        s.setBreakpoint(offs[i], {hit: function (frame) { g.log += 'B'; }});
    }
    g.log += 'A';
};

function test(s) {
    assertEq(s.charAt(s.length - 1) !== '\n', true);
    var count = s.split(/\n/).length;  // number of lines in s
    g.i = 0;
    g.log = '';
    where = 1 + count;
    g.eval("line0 = Error().lineNumber;\n" +
           "debugger;\n" +          // line0 + 1
           s +                      // line0 + 2 ... line0 + where
           " log += 'C';\n");
    assertEq(g.log, 'ABC');
}

function repeat(s) {
    return Array((1 << 14) + 1).join(s);  // 16K copies of s
}
var long_expr = "i" + repeat(" + i");
var long_throw_stmt = "throw " + long_expr + ";\n";

// long break (JSOP_GOTOX)
test("for (;;) {\n" +
     "    if (i === 0)\n" +
     "        break;\n" +
     "    " + long_throw_stmt +
     "}");

// long continue (JSOP_GOTOX)
test("do {\n" +
     "    if (i === 0)\n" +
     "        continue;\n" +
     "    " + long_throw_stmt +
     "} while (i !== 0);");

// long if consequent (JSOP_IFEQX)
test("if (i === 2) {\n" +
     "    " + long_throw_stmt +
     "}");

// long catch-block with finally (JSOP_GOSUBX)
test("try {\n" +
     "    i = 0;\n" +
     "} catch (exc) {\n" +
     "    throw " + long_expr + ";\n" +
     "} finally {\n" +
     "    i = 1;\n" +
     "}");

// long case (JSOP_TABLESWITCHX)
test("switch (i) {\n" +
     "  default:\n" +
     "  case 1: " + long_throw_stmt +
     "  case 0: i++; }");

test("switch (i) {\n" +
     "  case 1: case 2: case 3: " + long_throw_stmt +
     "  default: i++; }");

// long case (JSOP_LOOKUPSWITCHX)
test("switch ('' + i) {\n" +
     "  default:\n" +
     "  case '1': " + long_throw_stmt +
     "  case '0': i++; }");

test("switch (i) {\n" +
     "  case '1': case '2': case '3': " + long_throw_stmt +
     "  default: i++; }");

// long case or case-expression (JSOP_CASEX)
test("switch (i) {\n" +
     "  case i + 1 - i:\n" +
     "  default:\n" +
     "    " + long_throw_stmt +
     "  case i + i:\n" +
     "    i++; break; }");

// long case when JSOP_CASE is used (JSOP_DEFAULTX)
test("switch (i) {\n" +
     "  case i + 1 - i:\n" +
     "    " + long_throw_stmt +
     "  default:\n" +
     "    i++; break; }");
