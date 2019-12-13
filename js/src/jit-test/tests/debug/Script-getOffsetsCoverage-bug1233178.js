gczeal(2);
g = newGlobal({newCompartment: true});
dbg = Debugger(g);
function loop() {
  for (var i = 0; i < 10; i++)
    debugger;
}
g.eval(loop.toString());
dbg.onDebuggerStatement = function(f) {
  f.script.getOffsetsCoverage();
}
dbg.collectCoverageInfo = true;
g.eval("loop")();
