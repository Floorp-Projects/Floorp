/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client, targetFront }) => {
    const onPaused = waitForEvent(threadFront, "paused");
    await threadFront.interrupt();
    await onPaused;
    Assert.equal(threadFront.paused, true);
    await threadFront.resume();
    Assert.equal(threadFront.paused, false);
  })
);
