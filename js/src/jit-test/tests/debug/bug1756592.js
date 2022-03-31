let g = newGlobal({ newCompartment: true});
let d = new Debugger;
g.eval("function foo() { invokeInterruptCallback(() => {}) }");

// Warp-compile.
setInterruptCallback(function() { return true; });
for (var i = 0; i < 20; i++) {
  g.foo();
}

setInterruptCallback(function() {
  d.addDebuggee(g)
  d.getNewestFrame().onStep = function() {
    d.removeDebuggee(g);
    return { return: 0 };
  }
  return true
});

g.foo();
