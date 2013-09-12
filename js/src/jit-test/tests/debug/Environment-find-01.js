// find sees that vars are hoisted out of with statements.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.environment.find("x").type, "with");
    hits++;
};

assertEq(g.eval("(function () {\n" +
                "    function g() { x = 1; }\n" +
                "    with ({x: 2}) {\n" +
                "        var x;\n" +
                "        debugger;\n" +
                "        return x;\n" +
                "    }\n" +
                "})();"), 2);
assertEq(hits, 1);
