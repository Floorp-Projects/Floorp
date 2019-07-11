/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client }) => {
    debuggee.eval(
      function stopMe(arg1) {
        debugger;
      }.toString()
    );

    await test_object_grip(debuggee, threadFront);
  })
);

async function test_object_grip(debuggee, threadFront) {
  const code = `
    stopMe({
      get prop(){
        debugger;
      },
    });
  `;
  const objClient = await eval_and_resume(
    debuggee,
    threadFront,
    code,
    async frame => {
      const arg1 = frame.arguments[0];
      Assert.equal(arg1.class, "Object");

      const obj = threadFront.pauseGrip(arg1);
      await obj.threadGrip();
      return obj;
    }
  );

  // Ensure that we actually paused at the `debugger;` line.
  await Promise.all([
    wait_for_pause(threadFront, frame => {
      Assert.equal(frame.where.line, 4);
      Assert.equal(frame.where.column, 8);
    }),
    objClient.getPropertyValue("prop", null),
  ]);
}

function eval_and_resume(debuggee, threadFront, code, callback) {
  return new Promise((resolve, reject) => {
    wait_for_pause(threadFront, callback).then(resolve, reject);

    // This synchronously blocks until 'threadFront.resume()' above runs
    // because the 'paused' event runs everthing in a new event loop.
    debuggee.eval(code);
  });
}

function wait_for_pause(threadFront, callback = () => {}) {
  return new Promise((resolve, reject) => {
    threadFront.once("paused", function(packet) {
      (async () => {
        try {
          return await callback(packet.frame);
        } finally {
          await threadFront.resume();
        }
      })().then(resolve, reject);
    });
  });
}
