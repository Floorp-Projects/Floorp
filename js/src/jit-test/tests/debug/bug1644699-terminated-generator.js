// |jit-test| exitstatus:6
// Ensure that a frame terminated due to an interrupt in the generator
// builtin will properly be treated as terminated.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);
g.eval(`
var done = false;
async function* f() {
  await null;
  await null;
  await null;
  await null;
  done = true;
}
`);

dbg.onEnterFrame = f => {
  frame = f;
  dbg.onEnterFrame = undefined;
};

setInterruptCallback(function() {
  const stack = saveStack();

  // We want to explicitly terminate inside the AsyncGeneratorNext builtin
  // when it tries to resume execution at the 'await' in the async generator.
  // Terminating inside AsyncGeneratorNext causes the generator to be closed,
  // and for this test case we specifically need that to happen without
  // entering the async generator frame because we're aiming to trigger the
  // case where DebugAPI::onLeaveFrame does not having the opportunity to
  // clean up the generator information associated with the Debugger.Frame.
  if (
    stack.parent &&
    stack.parent.source === "self-hosted" &&
    stack.parent.functionDisplayName === "AsyncGeneratorNext" &&
    stack.parent.parent &&
    stack.parent.parent.source === stack.source &&
    stack.parent.parent.line === DRAIN_QUEUE_LINE
  ) {
    return false;
  }

  // Keep interrupting until we find the right place.
  interruptIf(true);
  return true;
});

// Run the async generator and suspend at the first await.
const it = g.f();
let promise = it.next();

// Queue the interrupt so that it will start trying to terminate inside the
// generator builtin.
interruptIf(true);

// Explicitly drain the queue to run the async generator to completion.
const DRAIN_QUEUE_LINE = saveStack().line + 1;
drainJobQueue();

let threw = false;
try {
  // In the original testcase for this bug, this call would cause
  // an assertion failure because the generator was closed.
  frame.environment;
} catch (err) {
  threw = true;
}
assertEq(threw, true);

// The frame here still has a GeneratorInfo datastructure because the
// termination interrupt will cause the generator to be closed without
// clearing that data. The frame must still be treated as terminated in
// this case in order for the Debugger API to behave consistently.
assertEq(frame.terminated, true);

// We should never reach the end of the async generator because it will
// have been terminated.
assertEq(g.done, false);
