// Setting frame.onStep to undefined turns off single-stepping.

var g = newGlobal();
g.a = 0;
g.eval("function f() {\n" +
       "    a++;\n" +
       "    a++;\n" +
       "    a++;\n" +
       "    a++;\n" +
       "    return a;\n" +
       "}\n");

var dbg = Debugger(g);
var seen = [0, 0, 0, 0, 0];
dbg.onEnterFrame = function (frame) {
    seen[g.a] = 1;
    frame.onStep = function () {
        seen[g.a] = 1;
        if (g.a === 2) {
            frame.onStep = undefined;
            assertEq(frame.onStep, undefined);
        }
    };
}

g.f();
assertEq(seen.join(","), "1,1,1,0,0");
