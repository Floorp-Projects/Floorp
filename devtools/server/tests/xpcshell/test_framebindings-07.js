/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const environment = await packet.frame.getEnvironment();
    Assert.equal(environment.type, "function");
    Assert.equal(environment.bindings.arguments[0].z.value, "z");

    const parent = environment.parent;
    Assert.equal(parent.type, "block");
    Assert.equal(parent.bindings.variables.banana3.value.class, "Function");

    const grandpa = parent.parent;
    Assert.equal(grandpa.type, "function");
    Assert.equal(grandpa.bindings.arguments[0].y.value, "y");

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
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
