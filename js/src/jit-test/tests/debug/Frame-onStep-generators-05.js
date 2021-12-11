// GC'ing a Debugger.Frame and its AbstractGeneratorObject adjusts the script's
// stepper count.

let g = newGlobal({newCompartment: true});
g.eval(`function* f() {}`);
for (let i = 0; i < 2; i++) {
  let dbg = new Debugger(g);
  dbg.onEnterFrame = frame => {
    frame.onStep = () => {};
  };
  g.f();
  dbg.onEnterFrame = undefined;
  gc();
}
