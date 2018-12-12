/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

add_task(threadClientTest(async ({ threadClient, debuggee, client, targetFront }) => {
  return new Promise(resolve => {
    threadClient.interrupt(function(response) {
      Assert.equal(threadClient.paused, true);
      threadClient.resume(function() {
        Assert.equal(threadClient.paused, false);
        resolve();
      });
    });
  });
}));
