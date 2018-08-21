// Returning {throw:} from an onPop handler when yielding works and
// closes the generator-iterator.

load(libdir + "iteration.js");

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
dbg.onDebuggerStatement = function handleDebugger(frame) {
    frame.onPop = function (c) {
        return {throw: "fit"};
    };
};
g.eval("function* g() { for (var i = 0; i < 10; i++) { debugger; yield i; } }");
g.eval("var it = g();");
var rv = gw.executeInGlobal("it.next();");
assertEq(rv.throw, "fit");

dbg.enabled = false;
assertIteratorDone(g.it);
