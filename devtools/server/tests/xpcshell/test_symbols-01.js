/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we can represent ES6 Symbols over the RDP.
 */

const URL = "foo.js";

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    await testSymbols(threadFront, debuggee);
  })
);

async function testSymbols(threadFront, debuggee) {
  const evalCode = () => {
    /* eslint-disable mozilla/var-only-at-top-level, no-unused-vars */
    // prettier-ignore
    Cu.evalInSandbox(
      "(" + function () {
        var symbolWithName = Symbol("Chris");
        var symbolWithoutName = Symbol();
        var iteratorSymbol = Symbol.iterator;
        debugger;
      } + "())",
      debuggee,
      "1.8",
      URL,
      1
    );
    /* eslint-enable mozilla/var-only-at-top-level, no-unused-vars */
  };

  const packet = await executeOnNextTickAndWaitForPause(evalCode, threadFront);
  const environment = await packet.frame.getEnvironment();
  const { symbolWithName, symbolWithoutName, iteratorSymbol } =
    environment.bindings.variables;

  equal(symbolWithName.value.type, "symbol");
  equal(symbolWithName.value.name, "Chris");

  equal(symbolWithoutName.value.type, "symbol");
  ok(!("name" in symbolWithoutName.value));

  equal(iteratorSymbol.value.type, "symbol");
  equal(iteratorSymbol.value.name, "Symbol.iterator");
}
