/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't run debuggee code when getting symbol names.
 */

const URL = "foo.js";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await testSymbols(threadFront, debuggee);
  })
);

async function testSymbols(threadFront, debuggee) {
  const evalCode = () => {
    /* eslint-disable mozilla/var-only-at-top-level, no-extend-native, no-unused-vars */
    // prettier-ignore
    Cu.evalInSandbox(
      "(" + function () {
        Symbol.prototype.toString = () => {
          throw new Error("lololol");
        };
        var sym = Symbol("le troll");
        debugger;
      } + "())",
      debuggee,
      "1.8",
      URL,
      1
    );
    /* eslint-enable mozilla/var-only-at-top-level, no-extend-native, no-unused-vars */
  };

  const packet = await executeOnNextTickAndWaitForPause(evalCode, threadFront);
  const environment = await packet.frame.getEnvironment();
  const { sym } = environment.bindings.variables;

  equal(sym.value.type, "symbol");
  equal(sym.value.name, "le troll");
}
