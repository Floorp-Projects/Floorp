// Throwing error after resolving async function's promise should not
// overwrite the promise's state or value/reason.
// This situation can happen either with debugger interaction or OOM.

// This testcase relies on the fact that there's a breakpoint after resolving
// the async function's promise, before leaving the async function's frame.
// This function searches for the last breakpoint before leaving the frame.
//
//  - `declCode` should declare an async function `f`, and the function should
//    set global variable `returning` to `true` just before return
//  - `callCode` should call the function `f` and make sure the function's
//    execution reaches the last breakpoint
function searchLastBreakpointBeforeReturn(declCode, callCode) {
  const g = newGlobal({ newCompartment: true });
  const dbg = new Debugger(g);
  g.eval(declCode);

  let hit = false;
  let offset = 0;
  dbg.onEnterFrame = function(frame) {
    if (frame.callee && frame.callee.name == "f") {
      frame.onStep = () => {
        if (!g.returning) {
          return undefined;
        }

        offset = frame.offset;
        return undefined;
      };
    }
  };

  g.eval(callCode);

  drainJobQueue();

  assertEq(offset != 0, true);

  return offset;
}

function testWithoutAwait() {
  const declCode = `
  var returning = false;
  async function f() {
    return (returning = true, "expected");
  };
  `;

  const callCode = `
  var p = f();
  `;

  const offset = searchLastBreakpointBeforeReturn(declCode, callCode);

  const g = newGlobal({ newCompartment: true });
  const dbg = new Debugger(g);
  g.eval(declCode);

  let onPromiseSettledCount = 0;
  dbg.onPromiseSettled = function(promise) {
    onPromiseSettledCount++;
    // No promise should be rejected.
    assertEq(promise.promiseState, "fulfilled");

    // Async function's promise should have expected value.
    if (onPromiseSettledCount == 1) {
      assertEq(promise.promiseValue, "expected");
    }
  };

  let hitBreakpoint = false;
  dbg.onEnterFrame = function(frame) {
    if (frame.callee && frame.callee.name == "f") {
      dbg.onEnterFrame = undefined;
      frame.script.setBreakpoint(offset, {
        hit() {
          hitBreakpoint = true;
          return { throw: "unexpected" };
        }
      });
    }
  };

  enableLastWarning();

  g.eval(`
  var fulfilledValue;
  var rejected = false;
  `);

  g.eval(callCode);

  // The execution reaches to the last breakpoint without running job queue.
  assertEq(hitBreakpoint, true);

  const warn = getLastWarning();
  assertEq(warn.message,
           "unhandlable error after resolving async function's promise");
  clearLastWarning();

  // Add reaction handler after resolution.
  // This handler's job will be enqueued immediately.
  g.eval(`
  p.then(x => {
    fulfilledValue = x;
  }, e => {
    rejected = true;
  });
  `);

  // Run the above handler.
  drainJobQueue();

  assertEq(g.fulfilledValue, "expected");
  assertEq(onPromiseSettledCount >= 1, true);
}

function testWithAwait() {
  const declCode = `
  var resolve;
  var p = new Promise(r => { resolve = r });
  var returning = false;
  async function f() {
    await p;
    return (returning = true, "expected");
  };
  `;

  const callCode = `
  var p = f();
  `;

  const resolveCode = `
  resolve("resolve");
  `;

  const offset = searchLastBreakpointBeforeReturn(declCode,
                                                  callCode + resolveCode);

  const g = newGlobal({newCompartment: true});
  const dbg = new Debugger(g);
  g.eval(declCode);

  let onPromiseSettledCount = 0;
  dbg.onPromiseSettled = function(promise) {
    onPromiseSettledCount++;

    // No promise should be rejected.
    assertEq(promise.promiseState, "fulfilled");

    // Async function's promise should have expected value.
    if (onPromiseSettledCount == 2) {
      assertEq(promise.promiseValue, "expected");
    }
  };

  let hitBreakpoint = false;
  dbg.onEnterFrame = function(frame) {
    if (frame.callee && frame.callee.name == "f") {
      dbg.onEnterFrame = undefined;
      frame.script.setBreakpoint(offset, {
        hit() {
          hitBreakpoint = true;
          return { throw: "unexpected" };
        }
      });
    }
  };

  enableLastWarning();

  g.eval(`
  var fulfilledValue1;
  var fulfilledValue2;
  var rejected = false;
  `);

  g.eval(callCode);

  assertEq(getLastWarning(), null);

  // Add reaction handler before resolution.
  // This handler's job will be enqueued when `p` is resolved.
  g.eval(`
  p.then(x => {
    fulfilledValue1 = x;
  }, e => {
    rejected = true;
  });
  `);

  g.eval(resolveCode);

  // Run the remaining part of async function, and the above handler.
  drainJobQueue();

  // The execution reaches to the last breakpoint after running job queue for
  // resolving `p`.
  assertEq(hitBreakpoint, true);

  const warn = getLastWarning();
  assertEq(warn.message,
           "unhandlable error after resolving async function's promise");
  clearLastWarning();

  assertEq(g.fulfilledValue1, "expected");
  assertEq(g.rejected, false);

  // Add reaction handler after resolution.
  // This handler's job will be enqueued immediately.
  g.eval(`
  p.then(x => {
    fulfilledValue2 = x;
  }, e => {
    rejected = true;
  });
  `);

  // Run the above handler.
  drainJobQueue();

  assertEq(g.fulfilledValue2, "expected");
  assertEq(g.rejected, false);
  assertEq(onPromiseSettledCount >= 3, true);
}

testWithoutAwait();
testWithAwait();
