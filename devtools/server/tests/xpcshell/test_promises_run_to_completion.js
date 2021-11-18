// Bug 1145201:  Promise then-handlers can still be executed while the debugger is paused.
//
// When a promise is resolved, for each of its callbacks, a microtask is queued
// to run the callback. At various points, the HTML spec says the browser must
// "perform a microtask checkpoint", which means to draw microtasks from the
// queue and run them, until the queue is empty.
//
// The HTML spec is careful to perform a microtask checkpoint directly after
// each invocation of an event handler or DOM callback, so that code using
// promises can trust that its promise callbacks run promptly, in a
// deterministic order, without DOM events or other outside influences
// intervening.
//
// When the JavaScript debugger interrupts the execution of debuggee content
// code, it naturally must process events for its own user interface and promise
// callbacks. However, it must not run any debuggee microtasks. The debuggee has
// been interrupted in the midst of executing some other code, and the
// JavaScript spec promises developers: "Once execution of a Job is initiated,
// the Job always executes to completion. No other Job may be initiated until
// the currently running Job completes." [1] This promise would be broken if the
// debugger's own event processing ran debuggee microtasks during the
// interruption.
//
// Looking at things from the other side, a microtask checkpoint must be
// performed before returning from a debugger callback, rather than being put
// off until the debuggee performs its next microtask checkpoint, so that
// debugger microtasks are not interleaved with debuggee microtasks. A debuggee
// microtask could hit a breakpoint or otherwise re-enter the debugger, which
// might be quite surprised to see a new debugger callback begin before its
// previous promise callbacks could finish.
//
// [1]: https://www.ecma-international.org/ecma-262/9.0/index.html#sec-jobs-and-job-queues

"use strict";

const Debugger = require("Debugger");

function test_promises_run_to_completion() {
  const g = createTestGlobal(
    "test global for test_promises_run_to_completion.js"
  );
  const dbg = new Debugger(g);
  g.Assert = Assert;
  const log = [""];
  g.log = log;

  dbg.onDebuggerStatement = function handleDebuggerStatement(frame) {
    dbg.onDebuggerStatement = undefined;

    // Exercise the promise machinery: resolve a promise and perform a microtask
    // queue. When called from a debugger hook, the debuggee's microtasks should not
    // run.
    log[0] += "debug-handler(";
    Promise.resolve(42).then(v => {
      Assert.equal(
        v,
        42,
        "debugger callback promise handler got the right value"
      );
      log[0] += "debug-react";
    });
    log[0] += "(";
    force_microtask_checkpoint();
    log[0] += ")";

    Promise.resolve(42).then(v => {
      // The microtask running this callback should be handled as we leave the
      // onDebuggerStatement Debugger callback, and should not be interleaved
      // with debuggee microtasks.
      log[0] += "(trailing)";
    });

    log[0] += ")";
  };

  // Evaluate some debuggee code that resolves a promise, and then enters the debugger.
  Cu.evalInSandbox(
    `
    log[0] += "eval(";
    Promise.resolve(42).then(function debuggeePromiseCallback(v) {
      Assert.equal(v, 42, "debuggee promise handler got the right value");
      // Debugger microtask checkpoints must not run debuggee microtasks, so
      // this callback should run at the next microtask checkpoint *not*
      // performed by the debugger.
      log[0] += "eval-react";
    });

    log[0] += "debugger(";
    debugger;
    log[0] += "))";
  `,
    g
  );

  // Let other microtasks run. This should run the debuggee's promise callback.
  log[0] += "final(";
  force_microtask_checkpoint();
  log[0] += ")";

  Assert.equal(
    log[0],
    `\
eval(\
debugger(\
debug-handler(\
(debug-react)\
)\
(trailing)\
))\
final(\
eval-react\
)`,
    "microtasks ran as expected"
  );

  run_next_test();
}

function force_microtask_checkpoint() {
  // Services.tm.spinEventLoopUntilEmpty only performs a microtask checkpoint if
  // there is actually an event to run. So make one up.
  let ran = false;
  Services.tm.dispatchToMainThread(() => {
    ran = true;
  });
  Services.tm.spinEventLoopUntil(
    "Test(test_promises_run_to_completion.js:force_microtask_checkpoint)",
    () => ran
  );
}

add_test(test_promises_run_to_completion);
