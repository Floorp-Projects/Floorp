/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  debuggee.eval(function stopMe() {
    debugger;
  }.toString());

  await test_object_grip(debuggee, threadClient);
}));

async function test_object_grip(debuggee, threadClient) {
  eval_and_resume(
    debuggee,
    threadClient,
    `
      var obj = {
        get getter() {
          return objects.indexOf(this);
        },
      };
      var objects = [obj, {}, [], new Boolean(), new Number(), new String()];
      stopMe(...objects);
    `,
    async frame => {
      const grips = frame.arguments;
      const objClient = threadClient.pauseGrip(grips[0]);
      const classes = ["Object", "Object", "Array", "Boolean", "Number", "String"];
      for (const [i, grip] of grips.entries()) {
        Assert.equal(grip.class, classes[i]);
        await check_getter(objClient, grip.actor, i);
      }
      await check_getter(objClient, null, 0);
      await check_getter(objClient, "invalid receiver actorId", 0);
    }
  );
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

async function check_getter(objClient, receiverId, expected) {
  const {value} = await objClient.getPropertyValue("getter", receiverId);
  Assert.equal(value.return, expected);
}
