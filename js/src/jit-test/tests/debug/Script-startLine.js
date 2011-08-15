var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var start, count;
dbg.onDebuggerStatement = function (frame) {
    assertEq(start, undefined);
    start = frame.script.startLine;
    count = frame.script.lineCount;
    assertEq(typeof frame.script.url, 'string');
};

function test(f) {
    start = count = g.first = g.last = undefined;
    f();
    assertEq(start, g.first);
    assertEq(count, g.last + 1 - g.first);
    print(start, count);
}

test(function () {
    g.eval("first = Error().lineNumber;\n" +
           "debugger;\n" +
           "last = Error().lineNumber;\n");
});

test(function () {
    g.evaluate("first = Error().lineNumber;\n" +
               "debugger;\n" +
               Array(17000).join("\n") +
               "last = Error().lineNumber;\n");
});

test(function () {
    g.eval("function f1() { first = Error().lineNumber\n" +
           "    debugger;\n" +
           "    last = Error().lineNumber; }\n" +
           "f1();");
});

g.eval("function f2() {\n" +
       "    eval('first = Error().lineNumber\\n\\ndebugger;\\n\\nlast = Error().lineNumber;');\n" +
       "}\n");
test(g.f2);
test(g.f2);
