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
      objClient.getProperty("x", function(response) {
        Assert.equal(response.descriptor.configurable, true);
        Assert.equal(response.descriptor.enumerable, true);
        Assert.equal(response.descriptor.writable, true);
        Assert.equal(response.descriptor.value, 10);

        objClient.getProperty("y", function(response) {
          Assert.equal(response.descriptor.configurable, true);
          Assert.equal(response.descriptor.enumerable, true);
          Assert.equal(response.descriptor.writable, true);
          Assert.equal(response.descriptor.value, "kaiju");

          objClient.getProperty("a", function(response) {
            Assert.equal(response.descriptor.configurable, true);
            Assert.equal(response.descriptor.enumerable, true);
            Assert.equal(response.descriptor.get.type, "object");
            Assert.equal(response.descriptor.get.class, "Function");
            Assert.equal(response.descriptor.set.type, "undefined");

            threadClient.resume().then(resolve);
          });
        });
      });
    });

    debuggee.eval(function stopMe(arg1) {
      debugger;
    }.toString());
    debuggee.eval("stopMe({ x: 10, y: 'kaiju', get a() { return 42; } })");
  });
}));
