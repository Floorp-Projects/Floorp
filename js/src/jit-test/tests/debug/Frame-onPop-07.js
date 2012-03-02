// Trying to set an onPop handler on a dead frame throws an exception.
var g = newGlobal('new-compartment');
g.eval("function f() { }");
g.eval("function g() { f(); }");
g.eval("function h() { g(); }");
g.eval("function i() { h(); }");
var dbg = new Debugger(g);
var log;

var frames = [];
dbg.onEnterFrame = function handleEnter(f) {
    log += "(";
    assertEq(f.live, true);
    frames.push(f);
};
log = '';
g.i();
assertEq(log, "((((");
assertEq(frames.length, 4);
for (i = 0; i < frames.length; i++) {
    assertEq(frames[i].live, false);
    var set = false;
    try {
        frames[i].onPop = function unappreciated() { };
        set = true; // don't assert in a 'try' block
    } catch (x) {
        assertEq(x instanceof Error, true);
    }
    assertEq(set, false);
}
