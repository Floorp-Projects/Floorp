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
    const { proxyTarget, proxyHandler } = await objClient.getProxySlots();

    strictEqual(grip.class, "Proxy", "Its a proxy grip.");
    strictEqual(
      proxyTarget.getGrip().class,
      "Proxy",
      "The target is also a proxy."
    );
    strictEqual(
      proxyHandler.getGrip().class,
      "Proxy",
      "The handler is also a proxy."
    );

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
    var proxy = new Proxy({}, {});
    for (let i = 0; i < 1e5; ++i)
      proxy = new Proxy(proxy, proxy);
    stopMe(proxy);
  `);
}
