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

    const objClient = threadFront.pauseGrip(args[0]);
    let response = await objClient.getProperty("x");
    Assert.equal(response.descriptor.configurable, true);
    Assert.equal(response.descriptor.enumerable, true);
    Assert.equal(response.descriptor.writable, true);
    Assert.equal(response.descriptor.value, 10);

    response = await objClient.getProperty("y");
    Assert.equal(response.descriptor.configurable, true);
    Assert.equal(response.descriptor.enumerable, true);
    Assert.equal(response.descriptor.writable, true);
    Assert.equal(response.descriptor.value, "kaiju");

    response = await objClient.getProperty("a");
    Assert.equal(response.descriptor.configurable, true);
    Assert.equal(response.descriptor.enumerable, true);
    Assert.equal(response.descriptor.get.type, "object");
    Assert.equal(response.descriptor.get.class, "Function");
    Assert.equal(response.descriptor.set.type, "undefined");

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
