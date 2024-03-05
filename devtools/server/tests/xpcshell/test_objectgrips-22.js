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

    const [grip] = packet.frame.arguments;
    const objClient = threadFront.pauseGrip(grip);
    const iterator = await objClient.enumSymbols();
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
  })
);

function evalCode(debuggee) {
  debuggee.eval(
    // These arguments are tested.
    // eslint-disable-next-line no-unused-vars
    function stopMe(arg1) {
      debugger;
    }.toString()
  );
  debuggee.eval(
    `stopMe(Object.defineProperty({}, Symbol("sym"), {value: 1}));`
  );
}
