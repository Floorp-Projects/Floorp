/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verify that when two of the "same" source are loaded concurrently (like e10s
 * frame scripts), breakpoints get hit in scripts defined by all sources.
 */

var gDebuggee;
var gClient;
var gTraceClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-breakpoints");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestThread(gClient, "test-breakpoints", testBreakpoint);
  });
  do_test_pending();
}

const testBreakpoint = Task.async(function* (threadResponse, tabClient, threadClient, tabResponse) {
  evalSetupCode();

  // Load the test source once.

  evalTestCode();
  equal(gDebuggee.functions.length, 1,
        "The test code should have added a function.");

  // Set a breakpoint in the test source.

  const source = yield getSource(threadClient, "test.js");
  const [response, bpClient] = yield setBreakpoint(source, {
    line: 3
  });
  ok(!response.error, "Shouldn't get an error setting the BP.");
  ok(!response.actualLocation,
     "Shouldn't get an actualLocation, the location we provided was good.");
  const bpActor = response.actor;

  yield resume(threadClient);

  // Load the test source again.

  evalTestCode();
  equal(gDebuggee.functions.length, 2,
        "The test code should have added another function.");

  // Should hit our breakpoint in a script defined by the first instance of the
  // test source.

  const bpPause1 = yield executeOnNextTickAndWaitForPause(gDebuggee.functions[0],
                                                          gClient);
  equal(bpPause1.why.type, "breakpoint",
        "Should pause because of hitting our breakpoint (not debugger statement).");
  equal(bpPause1.why.actors[0], bpActor,
        "And the breakpoint actor should be correct.");
  const dbgStmtPause1 = yield executeOnNextTickAndWaitForPause(() => resume(threadClient),
                                                               gClient);
  equal(dbgStmtPause1.why.type, "debuggerStatement",
        "And we should hit the debugger statement after the pause.");
  yield resume(threadClient);

  // Should also hit our breakpoint in a script defined by the second instance
  // of the test source.

  const bpPause2 = yield executeOnNextTickAndWaitForPause(gDebuggee.functions[1],
                                                          gClient);
  equal(bpPause2.why.type, "breakpoint",
        "Should pause because of hitting our breakpoint (not debugger statement).");
  equal(bpPause2.why.actors[0], bpActor,
        "And the breakpoint actor should be correct.");
  const dbgStmtPause2 = yield executeOnNextTickAndWaitForPause(() => resume(threadClient),
                                                               gClient);
  equal(dbgStmtPause2.why.type, "debuggerStatement",
        "And we should hit the debugger statement after the pause.");

  finishClient(gClient);
});

function evalSetupCode() {
  Cu.evalInSandbox(
    "this.functions = [];",
    gDebuggee,
    "1.8",
    "setup.js",
    1
  );
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
