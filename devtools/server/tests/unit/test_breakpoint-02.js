/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check that setting breakpoints when the debuggee is running works.
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_breakpoint_running();
                           });
  });
}

function test_breakpoint_running() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let location = { line: gDebuggee.line0 + 3 };

    gThreadClient.resume();

    // Setting the breakpoint later should interrupt the debuggee.
    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      Assert.equal(packet.type, "paused");
      Assert.equal(packet.why.type, "interrupted");
    });

    let source = gThreadClient.source(packet.frame.where.source);
    source.setBreakpoint(location, function (response) {
      // Eval scripts don't stick around long enough for the breakpoint to be set,
      // so just make sure we got the expected response from the actor.
      Assert.notEqual(response.error, "noScript");

      do_execute_soon(function () {
        gClient.close().then(gCallback);
      });
    });
  });

  /* eslint-disable */
  Cu.evalInSandbox(
    "var line0 = Error().lineNumber;\n" +
    "debugger;\n" +
    "var a = 1;\n" +  // line0 + 2
    "var b = 2;\n",  // line0 + 3
    gDebuggee
  );
  /* eslint-enable */
}
