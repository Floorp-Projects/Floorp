/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  debuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  await test_object_grip(debuggee, threadClient);
}));

async function test_object_grip(debuggee, threadClient) {
  const code = `
    stopMe({
      method: {},
    });
  `;
  const obj = await eval_and_resume(debuggee, threadClient, code, async frame => {
    const arg1 = frame.arguments[0];
    Assert.equal(arg1.class, "Object");

    await threadClient.pauseGrip(arg1).threadGrip();
    return arg1;
  });
  const objClient = threadClient.pauseGrip(obj);

  const method = threadClient.pauseGrip(
    (await objClient.getPropertyValue("method", null)).value.return,
  );

  try {
    await method.apply(obj, []);
    Assert.ok(false, "expected exception");
  } catch (err) {
    Assert.equal(err.message, "debugee object is not callable");
  }
}

function eval_and_resume(debuggee, threadClient, code, callback) {
  return new Promise((resolve, reject) => {
    wait_for_pause(threadClient, callback).then(resolve, reject);

    // This synchronously blocks until 'threadClient.resume()' above runs
    // because the 'paused' event runs everthing in a new event loop.
    debuggee.eval(code);
  });
}

function wait_for_pause(threadClient, callback = () => {}) {
  return new Promise((resolve, reject) => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      (async () => {
        try {
          return await callback(packet.frame);
        } finally {
          await threadClient.resume();
        }
      })().then(resolve, reject);
    });
  });
}
