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
    const packet = await executeOnNextTickAndWaitForPause(
      () => evalCode(debuggee),
      threadFront
    );

    const [grip] = packet.frame.arguments;
    strictEqual(
      grip.class,
      "Function",
      `Grip has expected value for "class" property`
    );
    strictEqual(
      grip.isClassConstructor,
      true,
      `Grip has expected value for "isClassConstructor" property`
    );

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(`
    class MyClass {};
    stopMe(MyClass);

    function stopMe(arg1) {
      debugger;
    }
  `);
}
