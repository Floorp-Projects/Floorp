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

    const [grip1, grip2] = packet.frame.arguments;
    strictEqual(grip1.class, "Promise", "promise1 has a promise grip.");
    strictEqual(grip2.class, "Promise", "promise2 has a promise grip.");

    const objClient1 = threadFront.pauseGrip(grip1);
    const objClient2 = threadFront.pauseGrip(grip2);
    const { promiseState: state1 } = await objClient1.getPromiseState();
    const { promiseState: state2 } = await objClient2.getPromiseState();

    strictEqual(state1.state, "fulfilled", "promise1 was fulfilled.");
    strictEqual(state1.value, objClient2, "promise1 fulfilled with promise2.");
    ok(!state1.hasOwnProperty("reason"), "promise1 has no rejection reason.");

    strictEqual(state2.state, "rejected", "promise2 was rejected.");
    strictEqual(state2.reason, objClient1, "promise2 rejected with promise1.");
    ok(!state2.hasOwnProperty("value"), "promise2 has no resolution value.");

    await threadFront.resume();
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    function stopMe(arg) {
      debugger;
    }.toString()
  );

  debuggee.eval(`
    var resolve;
    var promise1 = new Promise(r => {resolve = r});
    Object.setPrototypeOf(promise1, null);
    var promise2 = Promise.reject(promise1);
    promise2.catch(() => {});
    Object.setPrototypeOf(promise2, null);
    resolve(promise2);
    stopMe(promise1, promise2);
  `);
}
