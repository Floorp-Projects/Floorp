/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Client request stacks should span the entire process from before making the
 * request to handling the reply from the server.  The server frames are not
 * included, nor can they be in most cases, since the server can be a remote
 * device.
 */

var { executeSoon } = require("devtools/shared/DevToolsUtils");
var defer = require("devtools/shared/defer");
var Services = require("Services");

var asyncStackEnabled =
  Services.prefs.getBoolPref("javascript.options.asyncstack");

registerCleanupFunction(() => {
  Services.prefs.setBoolPref("javascript.options.asyncstack",
                             asyncStackEnabled);
});

add_task(function* () {
  Services.prefs.setBoolPref("javascript.options.asyncstack", true);

  yield waitForTick();

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
  let deferred = defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}
