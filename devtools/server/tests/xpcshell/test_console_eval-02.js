/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that bound functions can be eagerly evaluated.
 */

add_task(
  threadFrontTest(async ({ threadFront, targetFront, debuggee }) => {
    const consoleFront = await targetFront.getFront("console");
    await consoleFront.evaluateJSAsync(`
      var obj = [1, 2, 3];
      var fn = obj.includes.bind(obj, 2);
    `);

    const normalResult = await consoleFront.evaluateJSAsync("fn()", {
      eager: true,
    });
    Assert.equal(normalResult.result, true, "normal eval");
  })
);
