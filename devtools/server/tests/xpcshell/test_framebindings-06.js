/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const env = await packet.frame.getEnvironment();
    equal(env.type, "function");
    equal(env.function.displayName, "banana3");
    let parent = env.parent;
    equal(parent.type, "block");
    ok("banana3" in parent.bindings.variables);
    parent = parent.parent;
    equal(parent.type, "function");
    equal(parent.function.displayName, "banana2");
    parent = parent.parent;
    equal(parent.type, "block");
    ok("banana2" in parent.bindings.variables);
    parent = parent.parent;
    equal(parent.type, "function");
    equal(parent.function.displayName, "banana");

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
