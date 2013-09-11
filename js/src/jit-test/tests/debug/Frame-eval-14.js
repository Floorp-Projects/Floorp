// Test the corner case of accessing an unaliased variable of a block
// while the block is not live.

var g = newGlobal();
g.eval("function h() { debugger }");
g.eval("function f() { let (x = 1, y) { (function() { y = 0 })(); h() } }");
g.eval("var surprise = null");

var dbg = new Debugger(g);
dbg.onDebuggerStatement = function(hFrame) {
    var fFrame = hFrame.older;
    assertEq(fFrame.environment.getVariable('x'), 1);
    assertEq(fFrame.environment.getVariable('y'), 0);
    fFrame.eval("surprise = function() { return ++x }");
    assertEq(g.surprise(), 2);
}
g.f();
assertEq(g.surprise !== null, true);

// Either succeed or throw an error about 'x' not being live
try {
    assertEq(g.surprise(), 3);
} catch (e) {
    assertEq(e+'', 'Error: x is not live');
}
