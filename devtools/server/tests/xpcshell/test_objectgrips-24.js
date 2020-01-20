/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that ES6 classes grip have the expected properties.

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    debuggee.eval(
      function stopMe() {
        debugger;
      }.toString()
    );

    const tests = [
      {
        fn: `function(){}`,
        isAsync: false,
        isGenerator: false,
      },
      {
        fn: `async function(){}`,
        isAsync: true,
        isGenerator: false,
      },
      {
        fn: `function *(){}`,
        isAsync: false,
        isGenerator: true,
      },
      {
        fn: `async function *(){}`,
        isAsync: true,
        isGenerator: true,
      },
    ];

    for (const { fn, isAsync, isGenerator } of tests) {
      const packet = await executeOnNextTickAndWaitForPause(
        () => debuggee.eval(`stopMe(${fn})`),
        threadFront
      );
      const [grip] = packet.frame.arguments;
      strictEqual(grip.class, "Function");
      strictEqual(grip.isAsync, isAsync);
      strictEqual(grip.isGenerator, isGenerator);

      await threadFront.resume();
    }
  })
);
