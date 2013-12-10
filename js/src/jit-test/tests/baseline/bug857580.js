
if (getBuildConfiguration()['generational-gc'])
    quit();

gczeal(2);
var g = newGlobal();
var dbg = new Debugger(g);
dbg.onNewScript = function(script) {
    fscript = script.getChildScripts()[0];
}
g.eval("function f(x) { arguments[0] = 3; return x }");
fscript.setBreakpoint(0, {hit:function(frame) {
    assertEq(frame.eval('x').return, 1);
    gc();
    return {return:42};
}});
assertEq(g.f(1), 42);
