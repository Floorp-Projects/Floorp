/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(threadClientTest(async ({ threadClient, debuggee, client }) => {
  return new Promise(resolve => {
    threadClient.addOneTimeListener("paused", function(event, packet) {
      const args = packet.frame.arguments;

      Assert.equal(args[0].class, "Object");

      const objClient = threadClient.pauseGrip(args[0]);
      objClient.getPrototypeAndProperties(function(response) {
        Assert.equal(response.ownProperties.a.configurable, true);
        Assert.equal(response.ownProperties.a.enumerable, true);
        Assert.equal(response.ownProperties.a.writable, true);
        Assert.equal(response.ownProperties.a.value.type, "Infinity");

        Assert.equal(response.ownProperties.b.configurable, true);
        Assert.equal(response.ownProperties.b.enumerable, true);
        Assert.equal(response.ownProperties.b.writable, true);
        Assert.equal(response.ownProperties.b.value.type, "-Infinity");

        Assert.equal(response.ownProperties.c.configurable, true);
        Assert.equal(response.ownProperties.c.enumerable, true);
        Assert.equal(response.ownProperties.c.writable, true);
        Assert.equal(response.ownProperties.c.value.type, "NaN");

        Assert.equal(response.ownProperties.d.configurable, true);
        Assert.equal(response.ownProperties.d.enumerable, true);
        Assert.equal(response.ownProperties.d.writable, true);
        Assert.equal(response.ownProperties.d.value.type, "-0");

        threadClient.resume(resolve);
      });
    });

    debuggee.eval(function stopMe(arg1) {
      debugger;
    }.toString());
    debuggee.eval("stopMe({ a: Infinity, b: -Infinity, c: NaN, d: -0 })");
  });
}));

