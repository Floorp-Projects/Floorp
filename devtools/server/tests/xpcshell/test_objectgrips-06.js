/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that sealed objects report themselves as sealed in their
 * grip.
 */

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

    const obj1 = packet.frame.arguments[0];
    Assert.ok(obj1.sealed);

    const obj1Client = threadFront.pauseGrip(obj1);
    Assert.ok(obj1Client.isSealed);

    const obj2 = packet.frame.arguments[1];
    Assert.ok(!obj2.sealed);

    const obj2Client = threadFront.pauseGrip(obj2);
    Assert.ok(!obj2Client.isSealed);

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );
  /* eslint-disable no-undef */
  debuggee.eval(
    "(" +
      function() {
        const obj1 = {};
        Object.seal(obj1);
        stopMe(obj1, {});
      } +
      "())"
  );
  /* eslint-enable no-undef */
}
