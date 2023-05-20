/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that frozen objects report themselves as frozen in their
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
    Assert.ok(obj1.frozen);

    const obj1Client = threadFront.pauseGrip(obj1);
    Assert.ok(obj1Client.isFrozen);

    const obj2 = packet.frame.arguments[1];
    Assert.ok(!obj2.frozen);

    const obj2Client = threadFront.pauseGrip(obj2);
    Assert.ok(!obj2Client.isFrozen);

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
      function () {
        const obj1 = {};
        Object.freeze(obj1);
        stopMe(obj1, {});
      } +
      "())"
  );
  /* eslint-enable no-undef */
}
