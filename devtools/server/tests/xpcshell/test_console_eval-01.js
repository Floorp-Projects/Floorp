/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that is possible to evaluate JS with evaluation timeouts in place.
 */

add_task(
  threadFrontTest(async ({ commands }) => {
    await commands.scriptCommand.execute(`
      function fib(n) {
        if (n == 1 || n == 0) {
          return 1;
        }

        return fib(n-1) + fib(n-2)
      }
    `);

    const normalResult = await commands.scriptCommand.execute("fib(1)", {
      eager: true,
    });
    Assert.equal(normalResult.result, 1, "normal eval");

    const timeoutResult = await commands.scriptCommand.execute("fib(100)", {
      eager: true,
    });
    Assert.equal(typeof timeoutResult.result, "object", "timeout eval");
    Assert.equal(timeoutResult.result.type, "undefined", "timeout eval type");
  })
);
