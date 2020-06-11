// This test ensures that capturing a stack works when the Debugger.Frame
// being used for an eval has an async stack on the activation between
// the two debuggee frames.

const global = newGlobal({ newCompartment: true });
const dbg = new Debugger(global);
dbg.onDebuggerStatement = function() {
  const frame = dbg.getNewestFrame();

  // Capturing this stack inside the debugger compartment will populate the
  // LiveSavedFrameCache with a SavedFrame in the debugger compartment.
  const opts = {
    stack: saveStack(),
  };

  bindToAsyncStack(function() {
    // This captured stack needs to properly invalidate the LiveSavedFrameCache
    // for the frame and create a new one inside the debuggee compartment.
    frame.eval(`saveStack()`);
  }, opts)();
};
global.eval(`debugger;`);
