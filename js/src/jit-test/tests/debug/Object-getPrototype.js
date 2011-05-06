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
assertEq(Op.getPrototype(), null);
assertEq(dbgeval("({})").getPrototype(), Op);

var Ap = dbgeval("[]").getPrototype();
assertEq(Ap, dbgeval("Array.prototype"));
assertEq(Ap.getPrototype(), Op);

assertEq(dbgeval("Object").getPrototype(), dbgeval("Function.prototype"));
