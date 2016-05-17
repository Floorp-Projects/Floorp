/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we get the magic properties on Error objects.

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
  do_test_pending();
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getOwnPropertyNames(function (aResponse) {
      var opn = aResponse.ownPropertyNames;
      do_check_eq(opn.length, 4);
      opn.sort();
      do_check_eq(opn[0], "columnNumber");
      do_check_eq(opn[1], "fileName");
      do_check_eq(opn[2], "lineNumber");
      do_check_eq(opn[3], "message");

      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });

  });

  gDebuggee.eval("stopMe(new TypeError('error message text'))");
}

