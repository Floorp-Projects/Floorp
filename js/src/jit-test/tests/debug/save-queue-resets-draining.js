// The draining state is reset when saving the job queue.

let g = newGlobal({newCompartment: true});

let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

dbg.onDebuggerStatement = frame => {
  // Enqueue a new job from within the debugger while executing another job
  // from outside of the debugger.
  enqueueJob(function() {});
};

g.eval(`
  enqueueJob(function() {
    debugger;
  });
`);
