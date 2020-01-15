/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check basic newSource packet sent from server.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    Cu.evalInSandbox(
      function inc(n) {
        return n + 1;
      }.toString(),
      debuggee
    );

    const sourcePacket = await waitForEvent(threadFront, "newSource");

    Assert.ok(!!sourcePacket.source);
    Assert.ok(!!sourcePacket.source.url.match(/test_new_source-01.js$/));
  })
);
