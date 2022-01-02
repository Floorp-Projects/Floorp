// An onPop handler can return its input argument for async generators. The
// debugger correctly adjusts the state of the async generator object.

let g = newGlobal({newCompartment: true});
g.eval(`
    async function* f() {}
`);

let hits = 0;
let dbg = new Debugger(g);
dbg.onEnterFrame = frame => {
  frame.onPop = completion => {
      hits++;
      return completion;
  };
};

let it = g.f();
let p = it.next();

assertEq(hits, 1);
