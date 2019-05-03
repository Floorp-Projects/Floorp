/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

add_task(threadClientTest(async ({ threadClient, debuggee, client, targetFront }) => {
  await threadClient.interrupt();
  Assert.equal(threadClient.paused, true);
  await threadClient.resume();
  Assert.equal(threadClient.paused, false);
}));
