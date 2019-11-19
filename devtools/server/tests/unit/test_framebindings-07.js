/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gClient;
var gThreadFront;
const { EnvironmentFront } = require("devtools/shared/fronts/environment");

add_task(
  threadFrontTest(
    async ({ threadFront, debuggee, client }) => {
      gThreadFront = threadFront;
      gDebuggee = debuggee;
      gClient = client;
      test_banana_environment();
    },
    { waitForFinish: true }
  )
);

function test_banana_environment() {
  gThreadFront.once("paused", async function(packet) {
    const environment = await packet.frame.getEnvironment();
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
    threadFrontTestFinished();
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
