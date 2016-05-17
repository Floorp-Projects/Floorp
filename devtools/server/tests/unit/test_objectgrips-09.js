/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/**
 * This tests exercises getProtypesAndProperties message accepted
 * by a thread actor.
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
  gDebuggee = addTestGlobal("test-grips", aServer);
  gDebuggee.eval(function stopMe(arg1, arg2) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(aServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-grips", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_object_grip();
    });
  });
}

function test_object_grip()
{
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {
    let args = aPacket.frame.arguments;

    gThreadClient.getPrototypesAndProperties([args[0].actor, args[1].actor], function (aResponse) {
      let obj1 = aResponse.actors[args[0].actor];
      let obj2 = aResponse.actors[args[1].actor];
      do_check_eq(obj1.ownProperties.x.configurable, true);
      do_check_eq(obj1.ownProperties.x.enumerable, true);
      do_check_eq(obj1.ownProperties.x.writable, true);
      do_check_eq(obj1.ownProperties.x.value, 10);

      do_check_eq(obj1.ownProperties.y.configurable, true);
      do_check_eq(obj1.ownProperties.y.enumerable, true);
      do_check_eq(obj1.ownProperties.y.writable, true);
      do_check_eq(obj1.ownProperties.y.value, "kaiju");

      do_check_eq(obj2.ownProperties.z.configurable, true);
      do_check_eq(obj2.ownProperties.z.enumerable, true);
      do_check_eq(obj2.ownProperties.z.writable, true);
      do_check_eq(obj2.ownProperties.z.value, 123);

      do_check_true(obj1.prototype != undefined);
      do_check_true(obj2.prototype != undefined);

      gThreadClient.resume(function () {
        gClient.close(gCallback);
      });
    });

  });

  gDebuggee.eval("stopMe({ x: 10, y: 'kaiju'}, { z: 123 })");
}

