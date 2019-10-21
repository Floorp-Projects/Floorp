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
    return new Promise(resolve => {
      threadFront.once("paused", async function(packet) {
        const args = packet.frame.arguments;

        Assert.equal(args[0].class, "Object");

        const objClient = threadFront.pauseGrip(args[0]);
        let response = await objClient.getPrototype();
        Assert.ok(response.prototype != undefined);

        const protoClient = threadFront.pauseGrip(response.prototype);
        response = await protoClient.getOwnPropertyNames();
        Assert.equal(response.ownPropertyNames.length, 2);
        Assert.equal(response.ownPropertyNames[0], "b");
        Assert.equal(response.ownPropertyNames[1], "c");

        await threadFront.resume();
        resolve();
      });

      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );
      debuggee.eval(
        function Constr() {
          this.a = 1;
        }.toString()
      );
      debuggee.eval(
        "Constr.prototype = { b: true, c: 'foo' }; var o = new Constr(); stopMe(o)"
      );
    });
  })
);
