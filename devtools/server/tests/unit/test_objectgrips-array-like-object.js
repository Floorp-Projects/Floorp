/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that objects are labeled as ArrayLike only when they have sequential
// numeric keys, and if they have a length property, that it matches the number
// of numeric keys. (See Bug 1371936)

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

    // Currying test function so we don't have to pass the debuggee and clients
    const isArrayLike = object =>
      test_object_grip_is_array_like(debuggee, client, threadFront, object);

    equal(await isArrayLike({}), false, "An empty object is not ArrayLike");
    equal(
      await isArrayLike({ length: 0 }),
      false,
      "An object with only a length property is not ArrayLike"
    );
    equal(
      await isArrayLike({ 2: "two" }),
      false,
      "Object not starting at 0 is not ArrayLike"
    );
    equal(
      await isArrayLike({ 0: "zero", 2: "two" }),
      false,
      "Object with non-consecutive numeric keys is not ArrayLike"
    );
    equal(
      await isArrayLike({ 0: "zero", 2: "two", length: 2 }),
      false,
      "Object with non-consecutive numeric keys is not ArrayLike"
    );
    equal(
      await isArrayLike({ 0: "zero", 1: "one", 2: "two", three: 3 }),
      false,
      "Object with a non-numeric property other than `length` is not ArrayLike"
    );
    equal(
      await isArrayLike({ 0: "zero", 1: "one", 2: "two", three: 3, length: 3 }),
      false,
      "Object with a non-numeric property other than `length` is not ArrayLike"
    );
    equal(
      await isArrayLike({ 0: "zero", 1: "one", 2: "two", length: 30 }),
      false,
      "Object with a wrongful `length` property is not ArrayLike"
    );

    equal(await isArrayLike({ 0: "zero" }), true);
    equal(await isArrayLike({ 0: "zero", 1: "two" }), true);
    equal(
      await isArrayLike({ 0: "zero", 1: "one", 2: "two", length: 3 }),
      true
    );
  })
);

async function test_object_grip_is_array_like(
  debuggee,
  dbgClient,
  threadFront,
  object
) {
  return new Promise((resolve, reject) => {
    threadFront.once("paused", async function(packet) {
      const [grip] = packet.frame.arguments;
      await threadFront.resume();
      resolve(grip.preview.kind === "ArrayLike");
    });

    debuggee.eval(`
      stopMe(${JSON.stringify(object)});
    `);
  });
}
