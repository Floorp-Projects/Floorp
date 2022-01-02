// |jit-test| error: ReferenceError

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { };");
enableGeckoProfiling();

try {
  // Only the ARM simulator supports single step profiling.
  enableSingleStepProfiling();
} catch (e) {
  throw new ReferenceError;
}

enableSingleStepProfiling();
a()
