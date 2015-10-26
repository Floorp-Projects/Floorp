// Basic getOffsetLocation test, using Error.lineNumber as the gold standard.

var g = newGlobal();
var dbg = Debugger(g);
var hits;
dbg.onDebuggerStatement = function (frame) {
    var knownLine = frame.eval("line").return;
    assertEq(frame.script.getOffsetLocation(frame.offset).lineNumber, knownLine);
    hits++;
};

hits = 0;
g.eval("var line = new Error().lineNumber; debugger;");
assertEq(hits, 1);

hits = 0;
g.eval("var s = 2 + 2;\n" +
       "s += 2;\n" +
       "line = new Error().lineNumber; debugger;\n" +
       "s += 2;\n" +
       "s += 2;\n" +
       "line = new Error().lineNumber; debugger;\n" +
       "s += 2;\n" +
       "assertEq(s, 12);\n");
assertEq(hits, 2);
