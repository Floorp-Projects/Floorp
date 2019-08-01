/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Ensure that the debugger resume page execution when the connection drops
 * and when the target is detached.
 */

add_task(
  threadFrontTest(({ threadFront, client, debuggee, targetFront }) => {
    return new Promise(resolve => {
      threadFront.once("paused", async function(packet) {
        ok(true, "The page is paused");
        ok(!debuggee.foo, "foo is still false after we hit the breakpoint");

        await targetFront.detach();

        // `detach` will force the destruction of the thread actor, which,
        // will resume the page execution. But all of that seems to be
        // synchronous and we have to spin the event loop in order to ensure
        // having the content javascript to execute the resumed code.
        await new Promise(executeSoon);

        // Closing the connection will force the thread actor to resume page
        // execution
        ok(debuggee.foo, "foo is true after target's detach request");

        executeSoon(resolve);
      });

      /* eslint-disable */
      Cu.evalInSandbox(
        "var foo = false;\n" +
        "debugger;\n" +
        "foo = true;\n",
        debuggee
      );
      /* eslint-enable */
      ok(debuggee.foo, "foo is false at startup");
    });
  })
);

add_task(
  threadFrontTest(({ threadFront, client, debuggee }) => {
    return new Promise(resolve => {
      threadFront.once("paused", async function(packet) {
        ok(true, "The page is paused");
        ok(!debuggee.foo, "foo is still false after we hit the breakpoint");

        await client.close();

        // `detach` will force the destruction of the thread actor, which,
        // will resume the page execution. But all of that seems to be
        // synchronous and we have to spin the event loop in order to ensure
        // having the content javascript to execute the resumed code.
        await new Promise(executeSoon);

        // Closing the connection will force the thread actor to resume page
        // execution
        ok(debuggee.foo, "foo is true after client close");

        executeSoon(resolve);
        dump("resolved\n");
      });

      /* eslint-disable */
      Cu.evalInSandbox(
        "var foo = false;\n" +
        "debugger;\n" +
        "foo = true;\n",
        debuggee
      );
      /* eslint-enable */
      ok(debuggee.foo, "foo is false at startup");
    });
  })
);
