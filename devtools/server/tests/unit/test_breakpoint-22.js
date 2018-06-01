/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1333219 - make that setBreakpoint fails when script is not found
 * at the specified line.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-breakpoints", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient,
                           "test-breakpoints",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test();
                           });
  });
}

const test = async function() {
  // Populate the `ScriptStore` so that we only test that the script
  // is added through `onNewScript`
  await getSources(gThreadClient);

  const packet = await executeOnNextTickAndWaitForPause(evalCode, gClient);
  const source = gThreadClient.source(packet.frame.where.source);
  const location = {
    line: gDebuggee.line0 + 2
  };

  const [res, ] = await setBreakpoint(source, location);
  ok(!res.error);

  const location2 = {
    line: gDebuggee.line0 + 7
  };

  await source.setBreakpoint(location2).then(_ => {
    do_throw("no code shall not be found the specified line or below it");
  }, reason => {
    Assert.equal(reason.error, "noCodeAtLineColumn");
    ok(reason.message);
  });

  await resume(gThreadClient);
  finishClient(gClient);
};

function evalCode() {
  // Start a new script
  Cu.evalInSandbox(`
var line0 = Error().lineNumber;
function some_function() {
  // breakpoint is valid here -- it slides one line below (line0 + 2)
}
debugger;
// no breakpoint is allowed after the EOF (line0 + 6)
`, gDebuggee);
}
