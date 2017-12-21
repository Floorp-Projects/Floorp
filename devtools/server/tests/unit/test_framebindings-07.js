/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gClient;
var gThreadClient;

// Test that the EnvironmentClient's getBindings() method works as expected.
function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-bindings");

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function () {
    attachTestTabAndResume(gClient, "test-bindings",
                           function (response, tabClient, threadClient) {
                             gThreadClient = threadClient;
                             test_banana_environment();
                           });
  });
  do_test_pending();
}

function test_banana_environment() {
  gThreadClient.addOneTimeListener("paused", function (event, packet) {
    let environment = packet.frame.environment;
    Assert.equal(environment.type, "function");

    let parent = environment.parent;
    Assert.equal(parent.type, "block");

    let grandpa = parent.parent;
    Assert.equal(grandpa.type, "function");

    let envClient = gThreadClient.environment(environment);
    envClient.getBindings(response => {
      Assert.equal(response.bindings.arguments[0].z.value, "z");

      let parentClient = gThreadClient.environment(parent);
      parentClient.getBindings(response => {
        Assert.equal(response.bindings.variables.banana3.value.class, "Function");

        let grandpaClient = gThreadClient.environment(grandpa);
        grandpaClient.getBindings(response => {
          Assert.equal(response.bindings.arguments[0].y.value, "y");
          gThreadClient.resume(() => finishClient(gClient));
        });
      });
    });
  });

  gDebuggee.eval("function banana(x) {\n" +
                 "  return function banana2(y) {\n" +
                 "    return function banana3(z) {\n" +
                 "      debugger;\n" +
                 "    };\n" +
                 "  };\n" +
                 "}\n" +
                 "banana('x')('y')('z');\n");
}
