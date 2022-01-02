// Changing onStep while the function is dead is allowed.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

let steps = new Set();
dbg.onDebuggerStatement = function(frame) {
  // Setting 'onStep' while alive is allowed.
  steps.add("debugger 1");
  assertEq(frame.onStack, true);
  frame.onStep = function() {
    steps.add("onstep 1");
  };

  dbg.onDebuggerStatement = function() {
    // Clear the 'onStep' while dead.
    steps.add("debugger 2");
    assertEq(frame.onStack, false);

    // Clearing 'onStep' while dead is allowed.
    frame.onStep = undefined;

      // Setting 'onStep' while dead is allowed.
    frame.onStep = function() {
      steps.add("onstep 2");
    };
  };
};

g.eval(
  `
    function myGen() {
      debugger;
      print("make sure we have a place to step to inside the frame");
    }

    const g = myGen();

    debugger;
  `
);

assertDeepEq(Array.from(steps), [
  "debugger 1",
  "onstep 1",
  "debugger 2",
]);
