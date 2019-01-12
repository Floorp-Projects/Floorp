function x() { n; }
function f() {
  try  { x(); } catch(ex) {}
}
var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {};");
enableGeckoProfiling();
try {
  enableSingleStepProfiling();
} catch (e) {
  // Not all platforms support single-step profiling.
}
f();
f();
f();
