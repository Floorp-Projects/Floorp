// Test that the ErrorCopier copies the optional "cause" property of error objects.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);
let hits = 0;
dbg.onDebuggerStatement = function (frame) {
  hits++;

  // Use |getVariable()| so we can easily throw our custom error from the
  // with-statement scope.
  let caught;
  try {
    frame.environment.getVariable("x");
  } catch (e) {
    caught = e;
  }

  // The ErrorCopier copied error, so |caught| isn't equal to |g.error|.
  assertEq(caught !== g.error, true);

  // Ensure the "cause" property is correctly copied.
  assertEq(caught.cause, g.cause);
};

// The error must be same-compartment with the debugger compartment for the
// ErrorCopier.
g.eval(`
  var cause = new Object();
  var error = new Error("", {cause});
`);

// Scope must be outside of debugger compartment to avoid triggering a
// DebuggeeWouldRun error.
let scope = {
  get x() {
    throw g.error;
  }
};

g.eval(`
  function f(scope) {
    with (scope) {
      debugger;
    }
  }
`);

g.f(scope);

assertEq(hits, 1);
