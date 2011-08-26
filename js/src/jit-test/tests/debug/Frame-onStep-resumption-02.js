// If frame.onStep returns {throw:}, an exception is thrown in the debuggee.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
g.eval("function h() { debugger; }\n" +
       "function f() {\n" +
       "    h();\n" +
       "    return 'fail';\n" +
       "}\n");

var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    frame.older.onStep = function () { return {throw: "pass"}; };
};

assertThrowsValue(g.f, "pass");
