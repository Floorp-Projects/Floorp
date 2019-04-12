// Like Frame-onStep-generator-resumption-01.js, but bail out by throwing.

let g = newGlobal({newCompartment: true});
g.eval(`
  function* f() {
    yield 1;
  }
`);

// Try force-returning from one of the instructions in `f` before the initial
// yield. In detail:
//
// *   This test calls `g.f()` under the Debugger.
// *   It uses the Debugger to step `ttl` times.
//     If we reach the initial yield before stepping the `ttl`th time, we're done.
// *   Otherwise, the test tries to force-return from `f`.
// *   That's an error, so the uncaughtExceptionHook is called.
// *   The uncaughtExceptionHook returns a `throw` completion value.
//
// Returns `true` if we reached the initial yield, false otherwise.
//
// Note that this function is called in a loop so that every possible relevant
// value of `ttl` is tried once.
function test(ttl) {
  let dbg = new Debugger(g);
  let exiting = false;  // we ran out of time-to-live and have forced return
  let done = false;  // we reached the initial yield without forced return
  let reported = false;  // a TypeError was reported.

  dbg.onEnterFrame = frame => {
    assertEq(frame.callee.name, "f");
    dbg.onEnterFrame = undefined;
    frame.onStep = () => {
      if (ttl == 0) {
        exiting = true;
        // This test case never resumes the generator after the initial
        // yield. Therefore the initial yield has not happened yet. So this
        // force-return will be an error.
        return {return: "ponies"};
      }
      ttl--;
    };
    frame.onPop = completion => {
      if (!exiting)
        done = true;
    };
  };

  dbg.uncaughtExceptionHook = (exc) => {
    // When onStep returns an invalid resumption value,
    // the error is reported here.
    assertEq(exc instanceof TypeError, true);
    reported = true;
    return {throw: "FAIL"};  // Bail out of the test.
  };

  let result;
  let caught = undefined;
  try {
    result = g.f();
  } catch (exc) {
    caught = exc;
  }

  if (done) {
    assertEq(reported, false);
    assertEq(result instanceof g.f, true);
    assertEq(caught, undefined);
  } else {
    assertEq(reported, true);
    assertEq(caught, "FAIL");
  }

  dbg.enabled = false;
  return done;
}

for (let ttl = 0; !test(ttl); ttl++) {}
