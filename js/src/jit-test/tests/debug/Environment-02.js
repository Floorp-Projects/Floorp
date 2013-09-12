// The last Environment on the environment chain always has .type == "object" and .object === the global object.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.eval("function h() { debugger; }");
var hits = 0;
dbg.onDebuggerStatement = function (hframe) {
    var env = hframe.older.environment;
    while (env.parent)
        env = env.parent;
    assertEq(env.type, "object");
    assertEq(env.object, gw);
    hits++;
};

g.eval("h();");
g.eval("(function () { h(); return []; })();");
g.eval("with (Math) { h(-2 * PI); }");
assertEq(hits, 3);
