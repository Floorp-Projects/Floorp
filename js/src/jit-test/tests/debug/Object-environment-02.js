// The .environment of a function Debugger.Object is an Environment object.

var g = newGlobal()
var dbg = Debugger(g);
var hits = 0;
g.h = function () {
    var frame = dbg.getNewestFrame();
    var fn = frame.eval("j").return;
    assertEq(fn.environment instanceof Debugger.Environment, true);
    var closure = frame.eval("f").return;
    assertEq(closure.environment instanceof Debugger.Environment, true);
    var async_fun = frame.eval("m").return;
    assertEq(async_fun.environment instanceof Debugger.Environment, true);
    var async_iter = frame.eval("n").return;
    assertEq(async_iter.environment instanceof Debugger.Environment, true);
    hits++;
};
g.eval(`
   function j(a) {
       var f = function () { return a; };
       function* g() { }
       async function m() { }
       async function* n() { }
       h();
       return f;
   }
   j(0);
`);
assertEq(hits, 1);
