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
    objClient.getProperty("x", function(aResponse) {
      do_check_eq(aResponse.descriptor.configurable, true);
      do_check_eq(aResponse.descriptor.enumerable, true);
      do_check_eq(aResponse.descriptor.writable, true);
      do_check_eq(aResponse.descriptor.value, 10);

      objClient.getProperty("y", function(aResponse) {
        do_check_eq(aResponse.descriptor.configurable, true);
        do_check_eq(aResponse.descriptor.enumerable, true);
        do_check_eq(aResponse.descriptor.writable, true);
        do_check_eq(aResponse.descriptor.value, "kaiju");

        objClient.getProperty("a", function(aResponse) {
          do_check_eq(aResponse.descriptor.configurable, true);
          do_check_eq(aResponse.descriptor.enumerable, true);
          do_check_eq(aResponse.descriptor.get.type, "object");
          do_check_eq(aResponse.descriptor.get.class, "Function");
          do_check_eq(aResponse.descriptor.set.type, "undefined");

          gThreadClient.resume(function() {
            gClient.close(gCallback);
          });
        });
      });
    });

  });

  gDebuggee.eval("stopMe({ x: 10, y: 'kaiju', get a() { return 42; } })");
}

