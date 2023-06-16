/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test checks that objects which are not extensible report themselves as
 * such.
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

    const [f, s, ne, e] = packet.frame.arguments;
    const [fClient, sClient, neClient, eClient] = packet.frame.arguments.map(
      a => threadFront.pauseGrip(a)
    );

    Assert.ok(!f.extensible);
    Assert.ok(!fClient.isExtensible);

    Assert.ok(!s.extensible);
    Assert.ok(!sClient.isExtensible);

    Assert.ok(!ne.extensible);
    Assert.ok(!neClient.isExtensible);

    Assert.ok(e.extensible);
    Assert.ok(eClient.isExtensible);

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
        const f = {};
        Object.freeze(f);
        const s = {};
        Object.seal(s);
        const ne = {};
        Object.preventExtensions(ne);
        stopMe(f, s, ne, {});
      } +
      "())"
  );
  /* eslint-enable no-undef */
}
