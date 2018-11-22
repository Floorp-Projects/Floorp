/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests exercises getProtypesAndProperties message accepted
 * by a thread actor.
 */

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      const args = packet.frame.arguments;

      threadClient.getPrototypesAndProperties(
        [args[0].actor, args[1].actor], function(response) {
          const obj1 = response.actors[args[0].actor];
          const obj2 = response.actors[args[1].actor];
          Assert.equal(obj1.ownProperties.x.configurable, true);
          Assert.equal(obj1.ownProperties.x.enumerable, true);
          Assert.equal(obj1.ownProperties.x.writable, true);
          Assert.equal(obj1.ownProperties.x.value, 10);

          Assert.equal(obj1.ownProperties.y.configurable, true);
          Assert.equal(obj1.ownProperties.y.enumerable, true);
          Assert.equal(obj1.ownProperties.y.writable, true);
          Assert.equal(obj1.ownProperties.y.value, "kaiju");

          Assert.equal(obj2.ownProperties.z.configurable, true);
          Assert.equal(obj2.ownProperties.z.enumerable, true);
          Assert.equal(obj2.ownProperties.z.writable, true);
          Assert.equal(obj2.ownProperties.z.value, 123);

          Assert.ok(obj1.prototype != undefined);
          Assert.ok(obj2.prototype != undefined);

          threadClient.resume(resolve);
        });
    });

    debuggee.eval(function stopMe(arg1) {
      debugger;
    }.toString());
    debuggee.eval("stopMe({ x: 10, y: 'kaiju'}, { z: 123 })");
  });
}));

