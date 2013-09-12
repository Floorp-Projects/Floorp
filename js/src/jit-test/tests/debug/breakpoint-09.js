// Setting a breakpoint in an eval script that is not on the stack. Bug 746973.
// We don't assert that the breakpoint actually hits because that depends on
// the eval cache, an implementation detail.

var g = newGlobal();
var dbg = Debugger(g);
g.eval("function f() { return eval('2+2'); }");
var s;
dbg.onNewScript = function (script) { s = script; };
g.f();
for (var offset of s.getLineOffsets(s.startLine))
    s.setBreakpoint(offset, {hit: function () {}});
assertEq(g.f(), 4);
