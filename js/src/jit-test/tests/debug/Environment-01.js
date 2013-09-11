// A live Environment can observe the new variables introduced by ES5 non-strict direct eval.

var g = newGlobal();
g.eval("var x = 'global'; function f(s) { h(); eval(s); h(); }");
g.eval("function h() { debugger; }");
var dbg = Debugger(g);
var env = undefined;
var hits = 0;
dbg.onDebuggerStatement = function (hframe) {
    if (env === undefined) {
        // First debugger statement.
        env = hframe.older.environment;
        assertEq(env.find("x") !== env, true);
        assertEq(env.names().indexOf("x"), -1);
    } else {
        // Second debugger statement, post-eval.
        assertEq(env.find("x"), env);
        assertEq(env.names().indexOf("x") >= 0, true);
    }
    hits++;
};
g.f("var x = 'local';");
assertEq(hits, 2);
