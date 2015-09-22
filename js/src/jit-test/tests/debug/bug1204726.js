
setJitCompilerOption("ion.warmup.trigger", 1);
gczeal(4);
function test() {
  for (var res = false; !res; res = inIon()) {};
}
var g = newGlobal();
g.parent = this;
g.eval(`
  var dbg = new Debugger();
  var parentw = dbg.addDebuggee(parent);
  dbg.onIonCompilation = function (graph) {};
`);
test();
