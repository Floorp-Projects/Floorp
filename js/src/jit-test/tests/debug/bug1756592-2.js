// |jit-test| --fast-warmup; --no-threads

let g = newGlobal({ newCompartment: true});
let d = new Debugger;
g.eval("function foo() { invokeInterruptCallback(() => {}) }");
g.eval("function bar() { foo(); }");

// Warp-compile.
with ({}) {}
setInterruptCallback(function() { return true; });
for (var i = 0; i < 100; i++) {
  g.bar();
}

// Trigger a forced return from an inlined frame
setInterruptCallback(function() {
  d.addDebuggee(g)
  d.getNewestFrame().onStep = function() {
    d.removeDebuggee(g);
    return { return: 0 };
  }
  return true
});

g.bar();
