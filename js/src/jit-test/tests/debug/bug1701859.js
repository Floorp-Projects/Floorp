// |jit-test| --fast-warmup
var dbgGlobal1 = newGlobal({ newCompartment: true });
for (var i = 0; i < 25; ++i) {
  try {
    var dbg = new dbgGlobal1.Debugger;
    dbg.addDebuggee(this);
    dbg.collectCoverageInfo = true;

    var dbgGlobal2 = newGlobal({ newCompartment: true });
    dbgGlobal2.debuggeeGlobal = this;
    dbgGlobal2.eval("(" + function () { new Debugger(debuggeeGlobal); } + ")();");

    dbg.removeDebuggee(this);
    dbg = null;

    if ((i % 10) === 0) {
      gc();
    }
  } catch (ex) {}
}
