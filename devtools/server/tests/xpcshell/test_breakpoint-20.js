/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that when two of the "same" source are loaded concurrently (like e10s
 * frame scripts), breakpoints get hit in scripts defined by all sources.
 */

var gDebuggee;

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    gDebuggee = debuggee;
    await testBreakpoint(threadFront);
  })
);

const testBreakpoint = async function (threadFront) {
  evalSetupCode();

  // Load the test source once.

  evalTestCode();
  equal(
    gDebuggee.functions.length,
    1,
    "The test code should have added a function."
  );

  // Set a breakpoint in the test source.

  const source = await getSource(threadFront, "test.js");
  setBreakpoint(threadFront, { sourceUrl: source.url, line: 3 });

  // Load the test source again.

  evalTestCode();
  equal(
    gDebuggee.functions.length,
    2,
    "The test code should have added another function."
  );

  // Should hit our breakpoint in a script defined by the first instance of the
  // test source.

  const bpPause1 = await executeOnNextTickAndWaitForPause(
    gDebuggee.functions[0],
    threadFront
  );
  equal(
    bpPause1.why.type,
    "breakpoint",
    "Should pause because of hitting our breakpoint (not debugger statement)."
  );
  const dbgStmtPause1 = await executeOnNextTickAndWaitForPause(
    () => resume(threadFront),
    threadFront
  );
  equal(
    dbgStmtPause1.why.type,
    "debuggerStatement",
    "And we should hit the debugger statement after the pause."
  );
  await resume(threadFront);

  // Should also hit our breakpoint in a script defined by the second instance
  // of the test source.

  const bpPause2 = await executeOnNextTickAndWaitForPause(
    gDebuggee.functions[1],
    threadFront
  );
  equal(
    bpPause2.why.type,
    "breakpoint",
    "Should pause because of hitting our breakpoint (not debugger statement)."
  );
  const dbgStmtPause2 = await executeOnNextTickAndWaitForPause(
    () => resume(threadFront),
    threadFront
  );
  equal(
    dbgStmtPause2.why.type,
    "debuggerStatement",
    "And we should hit the debugger statement after the pause."
  );
};

function evalSetupCode() {
  Cu.evalInSandbox("this.functions = [];", gDebuggee, "1.8", "setup.js", 1);
}

function evalTestCode() {
  Cu.evalInSandbox(
    `                                 // 1
    this.functions.push(function () { // 2
      var setBreakpointHere = 1;      // 3
      debugger;                       // 4
    });                               // 5
    `,
    gDebuggee,
    "1.8",
    "test.js",
    1
  );
}
