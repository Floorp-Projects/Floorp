/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that setting a breakpoint in a line with multiple entry points
 * triggers no matter which entry point we reach.
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
                             test_child_breakpoint();
                           });
  });
}

function test_child_breakpoint() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let source = gThreadClient.source(packet.frame.where.source);
    let location = { line: gDebuggee.line0 + 3 };

    source.setBreakpoint(location, function (response, bpClient) {
      // actualLocation is not returned when breakpoints don't skip forward.
      Assert.equal(response.actualLocation, undefined);

      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        // Check the return value.
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.why.type, "breakpoint");
        Assert.equal(packet.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        Assert.equal(gDebuggee.i, 0);

        gThreadClient.addOneTimeListener("paused", function (event, packet) {
          // Check the return value.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          Assert.equal(gDebuggee.i, 1);

          // Remove the breakpoint.
          bpClient.remove(function (response) {
            gThreadClient.resume(function () {
              gClient.close().then(gCallback);
            });
          });
        });

        // Continue until the breakpoint is hit again.
        gThreadClient.resume();
      });
      // Continue until the breakpoint is hit.
      gThreadClient.resume();
    });
  });

  /* eslint-disable */
  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "debugger;\n" +                      // line0 + 1
                   "var a, i = 0;\n" +                  // line0 + 2
                   "for (i = 1; i <= 2; i++) {\n" +     // line0 + 3
                   "  a = i;\n" +                       // line0 + 4
                   "}\n",                               // line0 + 5
                   gDebuggee);
  /* eslint-enable */
}
