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
        const { iterator } = await objClient.enumSymbols();
        const { ownSymbols } = await iterator.slice(0, iterator.count);

        strictEqual(ownSymbols.length, 1, "There is 1 symbol property.");
        const { name, descriptor } = ownSymbols[0];
        strictEqual(name, "Symbol(sym)", "Got right symbol name.");
        deepEqual(
          descriptor,
          {
            configurable: false,
            enumerable: false,
            writable: false,
            value: 1,
          },
          "Got right property descriptor."
        );

        await threadFront.resume();
        resolve();
      });
      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );
      debuggee.eval(
        `stopMe(Object.defineProperty({}, Symbol("sym"), {value: 1}));`
      );
    });
  })
);
