/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gClient;
var gThreadFront;
const { EnvironmentFront } = require("devtools/shared/fronts/environment");

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

// Test that the EnvironmentFront's getBindings() method works as expected.
function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-bindings");

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-bindings", function(
      response,
      targetFront,
      threadFront
    ) {
      gThreadFront = threadFront;
      test_banana_environment();
    });
  });
  do_test_pending();
}

function test_banana_environment() {
  gThreadFront.once("paused", async function(packet) {
    const environment = packet.frame.environment;
    Assert.equal(environment.type, "function");

    const parent = environment.parent;
    Assert.equal(parent.type, "block");

    const grandpa = parent.parent;
    Assert.equal(grandpa.type, "function");

    const envClient = new EnvironmentFront(gClient, environment);
    let response = await envClient.getBindings();
    Assert.equal(response.arguments[0].z.value, "z");

    const parentClient = new EnvironmentFront(gClient, parent);
    response = await parentClient.getBindings();
    Assert.equal(response.variables.banana3.value.class, "Function");

    const grandpaClient = new EnvironmentFront(gClient, grandpa);
    response = await grandpaClient.getBindings();
    Assert.equal(response.arguments[0].y.value, "y");
    await gThreadFront.resume();
    finishClient(gClient);
  });

  gDebuggee.eval(
    "function banana(x) {\n" +
      "  return function banana2(y) {\n" +
      "    return function banana3(z) {\n" +
      '      eval("");\n' +
      "      debugger;\n" +
      "    };\n" +
      "  };\n" +
      "}\n" +
      "banana('x')('y')('z');\n"
  );
}
