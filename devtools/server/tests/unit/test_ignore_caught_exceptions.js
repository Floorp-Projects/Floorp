/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that setting ignoreCaughtExceptions will cause the debugger to ignore
 * caught exceptions, but not uncaught ones.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_pause_frame();
    });
  });
  do_test_pending();
}

function test_pause_frame()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
      do_check_eq(aPacket.why.type, "exception");
      do_check_eq(aPacket.why.exception, "bar");
      gThreadClient.resume(function () {
        finishClient(gClient);
      });
    });
    gThreadClient.pauseOnExceptions(true, true);
    gThreadClient.resume();
  });

  try {
    gDebuggee.eval("(" + function() {
      debugger;
      try {
        throw "foo";
      } catch (e) {}
      throw "bar";
    } + ")()");
  } catch (e) {}
}
