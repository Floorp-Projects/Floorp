// getVariable works on a heavyweight environment after control leaves its scope.

var g = newGlobal();
var dbg = Debugger(g);
var envs = [];
dbg.onDebuggerStatement = function (frame) {
    envs.push(frame.environment);
};
g.eval("var f;\n" +
       "for (var x = 0; x < 3; x++) {\n" +
       "    (function (x) {\n" +
       "        for (var y = 0; y < 3; y++) {\n" +
       "            (function (z) {\n" +
       "                eval(z); // force heavyweight\n" +
       "                debugger;\n" +
       "            })(x + y);\n" +
       "        }\n" +
       "    })(x);\n" +
       "}");

var i = 0;
for (var x = 0; x < 3; x++) {
    for (var y = 0; y < 3; y++) {
        var e = envs[i++];
        assertEq(e.getVariable("z"), x + y);
    }
}
