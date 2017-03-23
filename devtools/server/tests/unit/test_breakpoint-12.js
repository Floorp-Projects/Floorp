/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Make sure that setting a breakpoint twice in a line without bytecodes works
 * as expected.
 */

const NUM_BREAKPOINTS = 10;
var gDebuggee;
var gClient;
var gThreadClient;
var gBpActor;
var gCount;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(server, callback) {
  gCallback = callback;
  gCount = 1;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-stack", server);
  gClient = new DebuggerClient(server.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_child_skip_breakpoint();
                           });
  });
}

function test_child_skip_breakpoint() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let source = gThreadClient.source(packet.frame.where.source);
    let location = { line: gDebuggee.line0 + 3};

    source.setBreakpoint(location, function (response, bpClient) {
      // Check that the breakpoint has properly skipped forward one line.
      do_check_eq(response.actualLocation.source.actor, source.actor);
      do_check_eq(response.actualLocation.line, location.line + 1);
      gBpActor = response.actor;

      // Set more breakpoints at the same location.
      set_breakpoints(source, location);
    });
  });

  /* eslint-disable */
  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "function foo() {\n" + // line0 + 1
                   "  this.a = 1;\n" +    // line0 + 2
                   "  // A comment.\n" +  // line0 + 3
                   "  this.b = 2;\n" +    // line0 + 4
                   "}\n" +                // line0 + 5
                   "debugger;\n" +        // line0 + 6
                   "foo();\n",            // line0 + 7
                   gDebuggee);
  /* eslint-enable */
}

// Set many breakpoints at the same location.
function set_breakpoints(source, location) {
  do_check_neq(gCount, NUM_BREAKPOINTS);
  source.setBreakpoint(location, function (response, bpClient) {
    // Check that the breakpoint has properly skipped forward one line.
    do_check_eq(response.actualLocation.source.actor, source.actor);
    do_check_eq(response.actualLocation.line, location.line + 1);
    // Check that the same breakpoint actor was returned.
    do_check_eq(response.actor, gBpActor);

    if (++gCount < NUM_BREAKPOINTS) {
      set_breakpoints(source, location);
      return;
    }

    // After setting all the breakpoints, check that only one has effectively
    // remained.
    gThreadClient.addOneTimeListener("paused", function (event, packet) {
      // Check the return value.
      do_check_eq(packet.type, "paused");
      do_check_eq(packet.frame.where.source.actor, source.actor);
      do_check_eq(packet.frame.where.line, location.line + 1);
      do_check_eq(packet.why.type, "breakpoint");
      do_check_eq(packet.why.actors[0], bpClient.actor);
      // Check that the breakpoint worked.
      do_check_eq(gDebuggee.a, 1);
      do_check_eq(gDebuggee.b, undefined);

      gThreadClient.addOneTimeListener("paused", function (event, packet) {
        // We don't expect any more pauses after the breakpoint was hit once.
        do_check_true(false);
      });
      gThreadClient.resume(function () {
        // Give any remaining breakpoints a chance to trigger.
        do_timeout(1000, function () {
          gClient.close().then(gCallback);
        });
      });
    });
    // Continue until the breakpoint is hit.
    gThreadClient.resume();
  });
}
