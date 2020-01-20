/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that reattaching to a previously detached thread works.
 */

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client, targetFront }) => {
    await threadFront.detach();
    Assert.equal(threadFront.state, "detached");
    const [, newThreadFront] = await targetFront.attachThread({});
    Assert.notEqual(threadFront, newThreadFront);
    Assert.equal(newThreadFront.state, "paused");
    Assert.equal(targetFront.threadFront, newThreadFront);
    await newThreadFront.resume();
  })
);
