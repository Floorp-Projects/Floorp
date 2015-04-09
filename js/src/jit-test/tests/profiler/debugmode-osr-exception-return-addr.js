// |jit-test| error: ReferenceError

var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { };");
enableSPSProfiling();

try {
  // Only the ARM simulator supports single step profiling.
  enableSingleStepProfiling();
} catch (e) {
  throw new ReferenceError;
}

enableSingleStepProfiling();
a()
