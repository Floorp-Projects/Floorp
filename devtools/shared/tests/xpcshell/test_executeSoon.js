/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Client request stacks should span the entire process from before making the
 * request to handling the reply from the server.  The server frames are not
 * included, nor can they be in most cases, since the server can be a remote
 * device.
 */

var { executeSoon } = require("resource://devtools/shared/DevToolsUtils.js");

add_task(async function () {
  await waitForTick();

  let stack = Components.stack;
  while (stack) {
    info(stack.name);
    if (stack.name == "waitForTick") {
      // Reached back to outer function before executeSoon
      ok(true, "Complete stack");
      return;
    }
    stack = stack.asyncCaller || stack.caller;
  }
  ok(false, "Incomplete stack");
});

function waitForTick() {
  return new Promise(resolve => {
    executeSoon(resolve);
  });
}
