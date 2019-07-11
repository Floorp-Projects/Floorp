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
    await new Promise(function(resolve) {
      threadFront.once("paused", async function(packet) {
        const [grip] = packet.frame.arguments;
        const objClient = threadFront.pauseGrip(grip);
        const { proxyTarget, proxyHandler } = await objClient.getProxySlots();

        strictEqual(grip.class, "Proxy", "Its a proxy grip.");
        strictEqual(proxyTarget.class, "Proxy", "The target is also a proxy.");
        strictEqual(
          proxyHandler.class,
          "Proxy",
          "The handler is also a proxy."
        );

        await threadFront.resume();
        resolve();
      });
      debuggee.eval(
        function stopMe(arg) {
          debugger;
        }.toString()
      );
      debuggee.eval(`
      var proxy = new Proxy({}, {});
      for (let i = 0; i < 1e5; ++i)
        proxy = new Proxy(proxy, proxy);
      stopMe(proxy);
    `);
    });
  })
);
