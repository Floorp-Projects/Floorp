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
    debuggee.eval(
      function stopMe(arg1) {
        debugger;
      }.toString()
    );
    const tests = [
      {
        value: true,
        class: "Boolean",
      },
      {
        value: 123,
        class: "Number",
      },
      {
        value: "foo",
        class: "String",
      },
      {
        value: Symbol("bar"),
        class: "Symbol",
        name: "bar",
      },
    ];
    for (const data of tests) {
      await new Promise(function(resolve) {
        threadFront.once("paused", async function(packet) {
          const [grip] = packet.frame.arguments;
          check_wrapped_primitive_grip(grip, data);

          await threadFront.resume();
          resolve();
        });
        debuggee.primitive = data.value;
        debuggee.eval("stopMe(Object(primitive));");
      });
    }
  })
);

function check_wrapped_primitive_grip(grip, data) {
  strictEqual(grip.class, data.class, "The grip has the proper class.");

  if (!grip.preview) {
    // In a worker thread Cu does not exist, the objects are considered unsafe and
    // can't be unwrapped, so there is no preview.
    return;
  }

  const value = grip.preview.wrappedValue;
  if (data.class === "Symbol") {
    strictEqual(
      value.type,
      "symbol",
      "The wrapped value grip has symbol type."
    );
    strictEqual(
      value.name,
      data.name,
      "The wrapped value grip has the proper name."
    );
  } else {
    strictEqual(value, data.value, "The wrapped value is the primitive one.");
  }
}
