// Changing onStep while the generator is suspended/dead is allowed.

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
    // Clear the 'onStep' while suspended.
    steps.add("debugger 2");
    assertEq(frame.onStack, false);

    // Clearing 'onStep' while suspended is allowed.
    frame.onStep = undefined;

      // Setting 'onStep' while suspended is allowed.
    frame.onStep = function() {
      steps.add("onstep 2");
    };

    dbg.onDebuggerStatement = function() {
      steps.add("debugger 3");
      assertEq(frame.onStack, false);

      // Clearing 'onStep' while dead is allowed.
      frame.onStep = undefined;

        // Setting 'onStep' while dead is allowed.
      frame.onStep = function() {
        steps.add("onstep 3");
      };
    };
  };
};

g.eval(
  `
    function* myGen() {
      debugger;
      yield;
    }

    const g = myGen();
    g.next();

    debugger;
    g.next();

    debugger;
  `
);

assertDeepEq(Array.from(steps), [
  "debugger 1",
  "onstep 1",
  "debugger 2",
  "onstep 2",
  "debugger 3",
]);
