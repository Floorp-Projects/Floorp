// Returning {throw:} from an onPop handler when yielding works and
// does not close the generator-iterator.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
dbg.onDebuggerStatement = function handleDebugger(frame) {
    frame.onPop = function (c) {
        return {throw: "fit"};
    };
};
g.eval("function g() { for (var i = 0; i < 10; i++) { debugger; yield i; } }");
g.eval("var it = g();");
var rv = gw.evalInGlobal("it.next();");
assertEq(rv.throw, "fit");

dbg.enabled = false;
assertEq(g.it.next(), 1);
