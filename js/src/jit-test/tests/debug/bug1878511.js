var c = 0;
var dbg = new Debugger();
oomTest(function () {
  if (c++ <= 20) {
    newGlobal({newCompartment: true});
  }
});
dbg.addAllGlobalsAsDebuggees();
