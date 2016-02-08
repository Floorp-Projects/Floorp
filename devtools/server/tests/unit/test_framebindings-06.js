/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-grips", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_banana_environment();
    });
  });
  do_test_pending();
}

function test_banana_environment()
{

  gThreadClient.addOneTimeListener("paused",
    function(aEvent, aPacket) {
      equal(aPacket.type, "paused");
      let env = aPacket.frame.environment;
      equal(env.type, "function");
      equal(env.function.name, "banana3");
      let parent = env.parent;
      equal(parent.type, "block");
      ok("banana3" in parent.bindings.variables);
      parent = parent.parent;
      equal(parent.type, "function");
      equal(parent.function.name, "banana2");
      parent = parent.parent;
      equal(parent.type, "block");
      ok("banana2" in parent.bindings.variables);
      parent = parent.parent;
      equal(parent.type, "function");
      equal(parent.function.name, "banana");

      gThreadClient.resume(function () {
                             finishClient(gClient);
                           });
    });

  gDebuggee.eval("\
        function banana(x) {                                            \n\
          return function banana2(y) {                                  \n\
            return function banana3(z) {                                \n\
              debugger;                                                 \n\
            };                                                          \n\
          };                                                            \n\
        }                                                               \n\
        banana('x')('y')('z');                                          \n\
        ");
}
