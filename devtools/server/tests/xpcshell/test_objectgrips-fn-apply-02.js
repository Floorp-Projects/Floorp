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

    const arg1 = packet.frame.arguments[0];
    Assert.equal(arg1.class, "Object");

    await threadFront.pauseGrip(arg1).threadGrip();
    const obj = arg1;
    await threadFront.resume();

    const objectFront = threadFront.pauseGrip(obj);

    const method = (await objectFront.getPropertyValue("method", null)).value
      .return;

    const methodCalled = method.apply(obj, []);

    // Ensure that we actually paused at the `debugger;` line.
    const packet2 = await waitForPause(threadFront);
    Assert.equal(packet2.frame.where.line, 4);
    Assert.equal(packet2.frame.where.column, 8);

    await threadFront.resume();
    await methodCalled;
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    function stopMe(arg1) {
      debugger;
    }.toString()
  );

  debuggee.eval(`
    stopMe({
      method(){
        debugger;
      },
    });
  `);
}
