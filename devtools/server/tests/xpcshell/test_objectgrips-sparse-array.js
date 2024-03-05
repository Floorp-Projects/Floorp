/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
    await threadFront.resume();

    strictEqual(grip.class, "Array", "The grip has an Array class");

    const { items } = grip.preview;
    strictEqual(items[0], null, "The empty slot has null as grip preview");
    deepEqual(
      items[1],
      { type: "undefined" },
      "The undefined value has grip value of type undefined"
    );
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    // These arguments are tested.
    // eslint-disable-next-line no-unused-vars
    function stopMe(arr) {
      debugger;
    }.toString()
  );
  debuggee.eval("stopMe([, undefined])");
}
