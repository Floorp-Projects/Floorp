/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that private field initialization is marked as effectful.
 */

add_task(
  threadFrontTest(async ({ threadFront, targetFront, debuggee }) => {
    const consoleFront = await targetFront.getFront("console");
    const initial_response = await consoleFront.evaluateJSAsync(`
      // This is a silly trick that allows stamping a field into
      // arbitrary objects.
      class Base { constructor(o) { return o; }};
      class A extends Base {
        #x = 10;
      };
      var obj = {};
    `);
    // If an exception occurred, private fields are not yet enabled. Abort
    // the rest of this test.
    if (initial_response.hasException) {
      return;
    }

    const eagerResult = await consoleFront.evaluateJSAsync("new A(obj)", {
      eager: true,
    });
    Assert.equal(
      eagerResult.result.type,
      "undefined",
      "shouldn't have actually produced a result"
    );

    const timeoutResult = await consoleFront.evaluateJSAsync("new A(obj); 1;");
    Assert.equal(timeoutResult.result, 1, "normal eval, no throw.");
  })
);
