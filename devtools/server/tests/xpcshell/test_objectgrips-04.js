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

    const args = packet.frame.arguments;

    Assert.equal(args[0].class, "Object");

    const objectFront = threadFront.pauseGrip(args[0]);
    const { ownProperties, prototype } =
      await objectFront.getPrototypeAndProperties();
    Assert.equal(ownProperties.x.configurable, true);
    Assert.equal(ownProperties.x.enumerable, true);
    Assert.equal(ownProperties.x.writable, true);
    Assert.equal(ownProperties.x.value, 10);

    Assert.equal(ownProperties.y.configurable, true);
    Assert.equal(ownProperties.y.enumerable, true);
    Assert.equal(ownProperties.y.writable, true);
    Assert.equal(ownProperties.y.value, "kaiju");

    Assert.equal(ownProperties.a.configurable, true);
    Assert.equal(ownProperties.a.enumerable, true);
    Assert.equal(ownProperties.a.get.getGrip().type, "object");
    Assert.equal(ownProperties.a.get.getGrip().class, "Function");
    Assert.equal(ownProperties.a.set.type, "undefined");

    Assert.ok(prototype != undefined);

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
  debuggee.eval("stopMe({ x: 10, y: 'kaiju', get a() { return 42; } })");
}
