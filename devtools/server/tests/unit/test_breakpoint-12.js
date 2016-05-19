/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  gCount = 1;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-stack", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_child_skip_breakpoint();
    });
  });
}

function test_child_skip_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = { line: gDebuggee.line0 + 3};

    source.setBreakpoint(location, function (aResponse, bpClient) {
      // Check that the breakpoint has properly skipped forward one line.
      do_check_eq(aResponse.actualLocation.source.actor, source.actor);
      do_check_eq(aResponse.actualLocation.line, location.line + 1);
      gBpActor = aResponse.actor;

      // Set more breakpoints at the same location.
      set_breakpoints(source, location);
    });

  });

  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "function foo() {\n" + // line0 + 1
                   "  this.a = 1;\n" +    // line0 + 2
                   "  // A comment.\n" +  // line0 + 3
                   "  this.b = 2;\n" +    // line0 + 4
                   "}\n" +                // line0 + 5
                   "debugger;\n" +        // line0 + 6
                   "foo();\n",            // line0 + 7
                   gDebuggee);
}

// Set many breakpoints at the same location.
function set_breakpoints(source, location) {
  do_check_neq(gCount, NUM_BREAKPOINTS);
  source.setBreakpoint(location, function (aResponse, bpClient) {
    // Check that the breakpoint has properly skipped forward one line.
    do_check_eq(aResponse.actualLocation.source.actor, source.actor);
    do_check_eq(aResponse.actualLocation.line, location.line + 1);
    // Check that the same breakpoint actor was returned.
    do_check_eq(aResponse.actor, gBpActor);

    if (++gCount < NUM_BREAKPOINTS) {
      set_breakpoints(source, location);
      return;
    }

    // After setting all the breakpoints, check that only one has effectively
    // remained.
    gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
      // Check the return value.
      do_check_eq(aPacket.type, "paused");
      do_check_eq(aPacket.frame.where.source.actor, source.actor);
      do_check_eq(aPacket.frame.where.line, location.line + 1);
      do_check_eq(aPacket.why.type, "breakpoint");
      do_check_eq(aPacket.why.actors[0], bpClient.actor);
      // Check that the breakpoint worked.
      do_check_eq(gDebuggee.a, 1);
      do_check_eq(gDebuggee.b, undefined);

      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        // We don't expect any more pauses after the breakpoint was hit once.
        do_check_true(false);
      });
      gThreadClient.resume(function () {
        // Give any remaining breakpoints a chance to trigger.
        do_timeout(1000, function () {
          gClient.close(gCallback);
        });
      });

    });
    // Continue until the breakpoint is hit.
    gThreadClient.resume();
  });

}
