/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that reattaching to a previously detached thread works.
 */

add_task(
  threadClientTest(async ({ threadClient, debuggee, client, targetFront }) => {
    await threadClient.detach();
    Assert.equal(threadClient.state, "detached");
    const [, newThreadClient] = await targetFront.attachThread({});
    Assert.notEqual(threadClient, newThreadClient);
    Assert.equal(newThreadClient.state, "paused");
    Assert.equal(targetFront.threadClient, newThreadClient);
    await newThreadClient.resume();
  })
);
