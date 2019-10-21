setJitCompilerOption("baseline.warmup.trigger", 0);
var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
dbg.addDebuggee(g);
g.eval("" + function f() { return 7; });
dbg.onEnterFrame = function() {
  dbg.removeDebuggee(g);
}
assertEq(g.f(), 7);
