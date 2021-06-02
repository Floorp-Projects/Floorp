/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that bound functions can be eagerly evaluated.
 */

add_task(
  threadFrontTest(async ({ commands }) => {
    await commands.scriptCommand.execute(`
      var obj = [1, 2, 3];
      var fn = obj.includes.bind(obj, 2);
    `);

    const normalResult = await commands.scriptCommand.execute("fn()", {
      eager: true,
    });
    Assert.equal(normalResult.result, true, "normal eval");
  })
);
