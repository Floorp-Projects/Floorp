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
      function stopMe() {
        debugger;
      }.toString()
    );

    await test_object_grip(debuggee, threadFront);
  })
);

async function test_object_grip(debuggee, threadFront) {
  const script = `
    var obj = {
      get getter() {
        return objects.indexOf(this);
      },
    };
    var objects = [obj, {}, [], new Boolean(), new Number(), new String()];
    stopMe(...objects);
  `;
  return new Promise(resolve => {
    threadFront.once("paused", async function(packet) {
      const { frame } = packet;
      try {
        const grips = frame.arguments;
        const objClient = threadFront.pauseGrip(grips[0]);
        const classes = [
          "Object",
          "Object",
          "Array",
          "Boolean",
          "Number",
          "String",
        ];
        for (const [i, grip] of grips.entries()) {
          Assert.equal(grip.class, classes[i]);
          await check_getter(objClient, grip.actor, i);
        }
        await check_getter(objClient, null, 0);
        await check_getter(objClient, "invalid receiver actorId", 0);
      } finally {
        await threadFront.resume();
        resolve();
      }
    });
    debuggee.eval(script);
  });
}

async function check_getter(objClient, receiverId, expected) {
  const { value } = await objClient.getPropertyValue("getter", receiverId);
  Assert.equal(value.return, expected);
}
