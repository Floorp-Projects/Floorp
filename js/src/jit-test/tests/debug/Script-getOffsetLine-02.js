// Frame.script/offset and Script.getOffsetLocation work in non-top frames.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var a = [];
    for (; frame.type == "call"; frame = frame.older)
        a.push(frame.script.getOffsetLocation(frame.offset).lineNumber - g.line0);
    assertEq(a.join(","), "1,2,3,4");
    hits++;
};
g.eval("var line0 = Error().lineNumber;\n" +
       "function f0() { debugger; }\n" +
       "function f1() { f0(); }\n" +
       "function f2() { f1(); }\n" +
       "function f3() { f2(); }\n" +
       "f3();\n");
assertEq(hits, 1);
