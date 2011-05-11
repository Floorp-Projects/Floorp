// |jit-test| debug
var g = newGlobal('new-compartment');
var dbgeval = function () {
        var dbg = new Debug(g);
        var hits = 0;
        g.eval("function f() { debugger; }");
        var lastval;
        dbg.hooks = {debuggerHandler: function (frame) { lastval = frame.arguments[0]; }};
        return function dbgeval(s) {
            g.eval("f(" + s + ");");
            return lastval;
        };
    }();

var Op = dbgeval("Object.prototype");
assertEq(Op.proto, null);
assertEq(dbgeval("({})").proto, Op);

var Ap = dbgeval("[]").proto;
assertEq(Ap, dbgeval("Array.prototype"));
assertEq(Ap.proto, Op);

assertEq(dbgeval("Object").proto, dbgeval("Function.prototype"));
