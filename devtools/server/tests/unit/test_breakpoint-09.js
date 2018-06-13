/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Check that removing a breakpoint works.
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
                             test_remove_breakpoint();
                           });
  });
}

function test_remove_breakpoint() {
  let done = false;
  gThreadClient.addOneTimeListener("paused", function(event, packet) {
    const source = gThreadClient.source(packet.frame.where.source);
    const location = { line: gDebuggee.line0 + 2 };

    source.setBreakpoint(location).then(function([response, bpClient]) {
      gThreadClient.addOneTimeListener("paused", function(event, packet) {
        // Check the return value.
        Assert.equal(packet.type, "paused");
        Assert.equal(packet.frame.where.source.actor, source.actor);
        Assert.equal(packet.frame.where.line, location.line);
        Assert.equal(packet.why.type, "breakpoint");
        Assert.equal(packet.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        Assert.equal(gDebuggee.a, undefined);

        // Remove the breakpoint.
        bpClient.remove(function(response) {
          done = true;
          gThreadClient.addOneTimeListener("paused",
                                           function(event, packet) {
            // The breakpoint should not be hit again.
                                             gThreadClient.resume(function() {
                                               Assert.ok(false);
                                             });
                                           });
          gThreadClient.resume();
        });
      });
      // Continue until the breakpoint is hit.
      gThreadClient.resume();
    });
  });

  /* eslint-disable */
  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "function foo(stop) {\n" + // line0 + 1
                   "  this.a = 1;\n" +        // line0 + 2
                   "  if (stop) return;\n" +  // line0 + 3
                   "  delete this.a;\n" +     // line0 + 4
                   "  foo(true);\n" +         // line0 + 5
                   "}\n" +                    // line0 + 6
                   "debugger;\n" +            // line1 + 7
                   "foo();\n",                // line1 + 8
                   gDebuggee);
  /* eslint-enable */
  if (!done) {
    Assert.ok(false);
  }
  gClient.close().then(gCallback);
}
