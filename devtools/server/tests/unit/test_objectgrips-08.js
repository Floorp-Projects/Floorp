/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    return new Promise(resolve => {
      threadFront.once("paused", function(packet) {
        const args = packet.frame.arguments;

        Assert.equal(args[0].class, "Object");

        const objClient = threadFront.pauseGrip(args[0]);
        objClient.getPrototypeAndProperties(function(response) {
          const { a, b, c, d, e, f, g } = response.ownProperties;
          testPropertyType(a, "Infinity");
          testPropertyType(b, "-Infinity");
          testPropertyType(c, "NaN");
          testPropertyType(d, "-0");
          testPropertyType(e, "BigInt");
          testPropertyType(f, "BigInt");
          testPropertyType(g, "BigInt");

          threadFront.resume().then(resolve);
        });
      });

      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );
      debuggee.eval(`stopMe({
      a: Infinity,
      b: -Infinity,
      c: NaN,
      d: -0,
      e: 1n,
      f: -2n,
      g: 0n,
      })`);
    });
  })
);

function testPropertyType(prop, expectedType) {
  Assert.equal(prop.configurable, true);
  Assert.equal(prop.enumerable, true);
  Assert.equal(prop.writable, true);
  Assert.equal(prop.value.type, expectedType);
}
