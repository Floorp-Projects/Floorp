/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that stepping in the last statement of the last frame doesn't
 * cause an unexpected pause, when another JS frame is pushed on the stack
 * (bug 785689).
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
      test_stepping_last();
    });
  });
}

function test_stepping_last()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
      // Check the return value.
      do_check_eq(aPacket.type, "paused");
      do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 2);
      do_check_eq(aPacket.why.type, "resumeLimit");
      // Check that stepping worked.
      do_check_eq(gDebuggee.a, undefined);
      do_check_eq(gDebuggee.b, undefined);

      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        // Check the return value.
        do_check_eq(aPacket.type, "paused");
        do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 3);
        do_check_eq(aPacket.why.type, "resumeLimit");
        // Check that stepping worked.
        do_check_eq(gDebuggee.a, 1);
        do_check_eq(gDebuggee.b, undefined);

        gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
          // Check the return value.
          do_check_eq(aPacket.type, "paused");
          // When leaving a stack frame the line number doesn't change.
          do_check_eq(aPacket.frame.where.line, gDebuggee.line0 + 3);
          do_check_eq(aPacket.why.type, "resumeLimit");
          // Check that stepping worked.
          do_check_eq(gDebuggee.a, 1);
          do_check_eq(gDebuggee.b, 2);

          gThreadClient.stepIn(function () {
            test_next_pause();
          });
        });
        gThreadClient.stepIn();
      });
      gThreadClient.stepIn();

    });
    gThreadClient.stepIn();

  });

  gDebuggee.eval("var line0 = Error().lineNumber;\n" +
                 "debugger;\n" +   // line0 + 1
                 "var a = 1;\n" +  // line0 + 2
                 "var b = 2;\n");  // line0 + 3
}

function test_next_pause()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    // Check the return value.
    do_check_eq(aPacket.type, "paused");
    // Before fixing bug 785689, the type was resumeLimit.
    do_check_eq(aPacket.why.type, "debuggerStatement");

    gThreadClient.resume(function () {
      gClient.close().then(gCallback);
    });
  });

  gDebuggee.eval("debugger;");
}
