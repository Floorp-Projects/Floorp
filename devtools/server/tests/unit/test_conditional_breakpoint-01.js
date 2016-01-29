/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check conditional breakpoint when condition evaluates to true.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-conditional-breakpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-conditional-breakpoint", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_simple_breakpoint();
    });
  });
  do_test_pending();
}

function test_simple_breakpoint()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let source = gThreadClient.source(aPacket.frame.where.source);
    source.setBreakpoint({
      line: 3,
      condition: "a === 1"
    }, function (aResponse, bpClient) {
      gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
        // Check the return value.
        do_check_eq(aPacket.why.type, "breakpoint");
        do_check_eq(aPacket.frame.where.line, 3);

        // Remove the breakpoint.
        bpClient.remove(function (aResponse) {
          gThreadClient.resume(function () {
            finishClient(gClient);
          });
        });

      });
      // Continue until the breakpoint is hit.
      gThreadClient.resume();

    });

  });

  Components.utils.evalInSandbox("debugger;\n" +   // 1
                                 "var a = 1;\n" +  // 2
                                 "var b = 2;\n",  // 3
                                 gDebuggee,
                                 "1.8",
                                 "test.js",
                                 1);
}
