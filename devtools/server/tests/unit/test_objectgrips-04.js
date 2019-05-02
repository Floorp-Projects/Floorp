/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

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
        Assert.equal(response.ownProperties.x.configurable, true);
        Assert.equal(response.ownProperties.x.enumerable, true);
        Assert.equal(response.ownProperties.x.writable, true);
        Assert.equal(response.ownProperties.x.value, 10);

        Assert.equal(response.ownProperties.y.configurable, true);
        Assert.equal(response.ownProperties.y.enumerable, true);
        Assert.equal(response.ownProperties.y.writable, true);
        Assert.equal(response.ownProperties.y.value, "kaiju");

        Assert.equal(response.ownProperties.a.configurable, true);
        Assert.equal(response.ownProperties.a.enumerable, true);
        Assert.equal(response.ownProperties.a.get.type, "object");
        Assert.equal(response.ownProperties.a.get.class, "Function");
        Assert.equal(response.ownProperties.a.set.type, "undefined");

        Assert.ok(response.prototype != undefined);

        const protoClient = threadClient.pauseGrip(response.prototype);
        protoClient.getOwnPropertyNames(function(response) {
          Assert.ok(response.ownPropertyNames.toString != undefined);

          threadClient.resume().then(resolve);
        });
      });
    });

    debuggee.eval(function stopMe(arg1) {
      debugger;
    }.toString());
    debuggee.eval("stopMe({ x: 10, y: 'kaiju', get a() { return 42; } })");
  });
}));

