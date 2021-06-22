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

    const obj = threadFront.pauseGrip(arg1);
    await obj.threadGrip();

    const objClient = obj;
    await threadFront.resume();

    const objClientCalled = objClient.getPropertyValue("prop", null);

    // Ensure that we actually paused at the `debugger;` line.
    const packet2 = await waitForPause(threadFront);
    Assert.equal(packet2.frame.where.line, 4);
    Assert.equal(packet2.frame.where.column, 8);

    await threadFront.resume();
    await objClientCalled;
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
      get prop(){
        debugger;
      },
    });
  `);
}
