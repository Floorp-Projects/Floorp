var g = newGlobal();
var dbg = Debugger(g);
var start, count;
dbg.onDebuggerStatement = function (frame) {
    assertEq(start, undefined);
    start = frame.script.startLine;
    count = frame.script.lineCount;
    assertEq(typeof frame.script.url, 'string');
};

function test(f, manualCount) {
    start = count = g.first = g.last = undefined;
    f();
    if (manualCount)
        g.last = g.first + manualCount - 1;
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

// Having a last = Error().lineNumber forces a setline srcnote, so test a
// function that ends with newline srcnotes.
g.eval("/* Any copyright is dedicated to the Public Domain.\n" +
       " http://creativecommons.org/publicdomain/zero/1.0/ */\n" +
       "\n" +
       "function secondCall() { first = Error().lineNumber;\n" +
       "    debugger;\n" +
       "    // Comment\n" +
       "    eval(\"42;\");\n" +
       "    function foo() {}\n" +
       "    if (true) {\n" +
       "        foo();\n" +  // <- this is +6 and must be within the extent
       "    }\n" +
       "}");
test(g.secondCall, 7);
