// |jit-test| --no-warp
// This is hitting bug 1505387 with WarpBuilder and eager compilation.

// LiveSavedFrameCache should not be confused by eval-in-frame-prev links.

const g = newGlobal({newCompartment: true});
const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

g.evaluate(`
  function inner() { debugger; }
  function outer() { inner(); }
`);

dbg.onDebuggerStatement = function handler(frame) {
  // Capture the JS stack, putting frames for handler, inner, and outer in the
  // cache. Perform all captures in g's compartment, to avoid flushing the cache
  // for compartment mismatches.
  frame.eval('print(`in inner: ${saveStack()}`)');

  // Carry out a capture in outer's context, following the eval-in-frame prev
  // link and thus skipping over inner, removing it from the cache.
  frame.older.eval('print(`in outer: ${saveStack()}`)');

  // Capture again. inner's hasCachedSavedFrame bit should be set, but its entry
  // has been flushed.
  frame.eval('print(`in inner: ${saveStack()}`)');
};

g.outer();
