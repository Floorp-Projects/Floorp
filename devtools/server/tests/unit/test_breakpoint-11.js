/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Make sure that setting a breakpoint in a line with bytecodes in multiple
 * scripts, sets the breakpoint in all of them (bug 793214).
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function() {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack",
                           function(response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_child_breakpoint();
                           });
  });
}

function test_child_breakpoint() {
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const source = gThreadClient.source(packet.frame.where.source);
    const location = { line: gDebuggee.line0 + 2 };

    source.setBreakpoint(location, function(response, bpClient) {
      // actualLocation is not returned when breakpoints don't skip forward.
      Assert.equal(response.actualLocation, undefined);

      gThreadClient.addOneTimeListener("paused", function(event, packet) {
        // Check the return value.
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.why.type, "breakpoint");
        Assert.equal(packet.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        Assert.equal(gDebuggee.a, undefined);

        gThreadClient.addOneTimeListener("paused", function(event, packet) {
          // Check the return value.
          Assert.equal(packet.type, "paused");
          Assert.equal(packet.why.type, "breakpoint");
          Assert.equal(packet.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          Assert.equal(gDebuggee.a.b, 1);
          Assert.equal(gDebuggee.res, undefined);

          // Remove the breakpoint.
          bpClient.remove(function(response) {
            gThreadClient.resume(function() {
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
                   "var a = { b: 1, f: function() { return 2; } };\n" + // line0+2
                   "var res = a.f();\n",               // line0 + 3
                   gDebuggee);
  /* eslint-enable */
}
