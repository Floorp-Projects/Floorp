/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  gDebuggee = addTestGlobal("test-grips", aServer);
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    do_check_eq(args[0].class, "Object");

    let objClient = gThreadClient.pauseGrip(args[0]);
    objClient.getPrototypeAndProperties(function(aResponse) {
      do_check_eq(aResponse.ownProperties.a.configurable, true);
      do_check_eq(aResponse.ownProperties.a.enumerable, true);
      do_check_eq(aResponse.ownProperties.a.writable, true);
      do_check_eq(aResponse.ownProperties.a.value.type, "Infinity");

      do_check_eq(aResponse.ownProperties.b.configurable, true);
      do_check_eq(aResponse.ownProperties.b.enumerable, true);
      do_check_eq(aResponse.ownProperties.b.writable, true);
      do_check_eq(aResponse.ownProperties.b.value.type, "-Infinity");

      do_check_eq(aResponse.ownProperties.c.configurable, true);
      do_check_eq(aResponse.ownProperties.c.enumerable, true);
      do_check_eq(aResponse.ownProperties.c.writable, true);
      do_check_eq(aResponse.ownProperties.c.value.type, "NaN");

      do_check_eq(aResponse.ownProperties.d.configurable, true);
      do_check_eq(aResponse.ownProperties.d.enumerable, true);
      do_check_eq(aResponse.ownProperties.d.writable, true);
      do_check_eq(aResponse.ownProperties.d.value.type, "-0");

      gThreadClient.resume(function() {
        gClient.close(gCallback);
      });
    });
  });

  gDebuggee.eval("stopMe({ a: Infinity, b: -Infinity, c: NaN, d: -0 })");
}

