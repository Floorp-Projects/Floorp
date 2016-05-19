/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check basic step-out functionality.
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
      test_simple_stepping();
    });
  });
}

function test_simple_stepping()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
      // Check the return value.
      do_check_eq(aPacket.type, "paused");
      do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 5);
      do_check_eq(aPacket.why.type, "resumeLimit");
      // Check that stepping worked.
      do_check_eq(gDebuggee.a, 1);
      do_check_eq(gDebuggee.b, 2);

      gThreadClient.resume(function () {
        gClient.close(gCallback);
      });
    });
    gThreadClient.stepOut();

  });

  gDebuggee.eval("var line0 = Error().lineNumber;\n" +
                 "function f() {\n" + // line0 + 1
                 "  debugger;\n" +    // line0 + 2
                 "  this.a = 1;\n" +  // line0 + 3
                 "  this.b = 2;\n" +  // line0 + 4
                 "}\n" +              // line0 + 5
                 "f();\n");           // line0 + 6
}
