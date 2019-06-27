// |jit-test| --no-ggc

var g = newGlobal({ newCompartment: true });
g.eval(`
  function* g3() {
    debugger;
  }

  function next() {
    g3().next();
  }

  function ggg() {
    gc(gc);
  }
`);

var dbg = new Debugger(g);
var frame;
dbg.onDebuggerStatement = f => {
  frame = f;
};

g.next();
g.ggg();
