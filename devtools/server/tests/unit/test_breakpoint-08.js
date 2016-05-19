/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that setting a breakpoint in a line without code in a child script
 * will skip forward, in a file with two scripts.
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
}

function run_test_with_server(aServer, aCallback)
{
  gCallback = aCallback;
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
    gThreadClient.eval(aPacket.frame.actor, "foo", function (aResponse) {
      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        let obj = gThreadClient.pauseGrip(aPacket.why.frameFinished.return);
        obj.getDefinitionSite(runWithBreakpoint);
      });
    });

    function runWithBreakpoint(aPacket) {
      let source = gThreadClient.source(aPacket.source);
      let location = { line: gDebuggee.line0 + 3 };

      source.setBreakpoint(location, function (aResponse, bpClient) {
        // Check that the breakpoint has properly skipped forward one line.
        do_check_eq(aResponse.actualLocation.source.actor, source.actor);
        do_check_eq(aResponse.actualLocation.line, location.line + 1);

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

          // Remove the breakpoint.
          bpClient.remove(function (aResponse) {
            gThreadClient.resume(function () {
              gClient.close(gCallback);
            });
          });
        });

        // Continue until the breakpoint is hit.
        gThreadClient.resume();
      });
    }
  });

  Cu.evalInSandbox("var line0 = Error().lineNumber;\n" +
                   "function foo() {\n" + // line0 + 1
                   "  this.a = 1;\n" +    // line0 + 2
                   "  // A comment.\n" +  // line0 + 3
                   "  this.b = 2;\n" +    // line0 + 4
                   "}\n",                 // line0 + 5
                   gDebuggee,
                   "1.7",
                   "script1.js");

  Cu.evalInSandbox("var line1 = Error().lineNumber;\n" +
                   "debugger;\n" +        // line1 + 1
                   "foo();\n",           // line1 + 2
                   gDebuggee,
                   "1.7",
                   "script2.js");
}
