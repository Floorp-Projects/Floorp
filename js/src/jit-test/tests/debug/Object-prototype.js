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
assertEq(Op.prototype, null);
assertEq(dbgeval("({})").prototype, Op);

var Ap = dbgeval("[]").prototype;
assertEq(Ap, dbgeval("Array.prototype"));
assertEq(Ap.prototype, Op);

assertEq(dbgeval("Object").prototype, dbgeval("Function.prototype"));
