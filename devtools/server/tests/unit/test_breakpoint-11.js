/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that setting a breakpoint in a line with bytecodes in multiple
 * scripts, sets the breakpoint in all of them (bug 793214).
 */

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test()
{
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
};

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
  initTestDebuggerServer(aServer);
  gDebuggee = addTestGlobal("test-stack", aServer);
  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-stack", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_child_breakpoint();
    });
  });
}

function test_child_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    let location = { line: gDebuggee.line0 + 2 };

    source.setBreakpoint(location, function (aResponse, bpClient) {
      // actualLocation is not returned when breakpoints don't skip forward.
      do_check_eq(aResponse.actualLocation, undefined);

      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        // Check the return value.
        do_check_eq(aPacket.type, "paused");
        do_check_eq(aPacket.why.type, "breakpoint");
        do_check_eq(aPacket.why.actors[0], bpClient.actor);
        // Check that the breakpoint worked.
        do_check_eq(gDebuggee.a, undefined);

        gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
          // Check the return value.
          do_check_eq(aPacket.type, "paused");
          do_check_eq(aPacket.why.type, "breakpoint");
          do_check_eq(aPacket.why.actors[0], bpClient.actor);
          // Check that the breakpoint worked.
          do_check_eq(gDebuggee.a.b, 1);
          do_check_eq(gDebuggee.res, undefined);

          // Remove the breakpoint.
          bpClient.remove(function (aResponse) {
            gThreadClient.resume(function () {
              gClient.close(gCallback);
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


  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "debugger;\n" +                      // line0 + 1
                   "var a = { b: 1, f: function() { return 2; } };\n" + // line0+2
                   "var res = a.f();\n",               // line0 + 3
                   gDebuggee);
}
