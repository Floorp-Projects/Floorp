/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the we can see console messages from the add-on

const ADDON_ID = "browser_dbg_addon4@tests.mozilla.org";
const ADDON_PATH = "addon4.xpi";

function getCachedMessages(webConsole) {
  let deferred = promise.defer();
  webConsole.getCachedMessages(["ConsoleAPI"], (aResponse) => {
    if (aResponse.error) {
      deferred.reject(aResponse.error);
      return;
    }
    deferred.resolve(aResponse.messages);
  });
  return deferred.promise;
}

function test() {
  Task.spawn(function* () {
    let addon = yield addTemporaryAddon(ADDON_PATH);
    let addonDebugger = yield initAddonDebugger(ADDON_ID);

    let webConsole = addonDebugger.webConsole;
    let messages = yield getCachedMessages(webConsole);
    is(messages.length, 1, "Should be one cached message");
    is(messages[0].arguments[0].type, "object", "Should have logged an object");
    is(messages[0].arguments[0].preview.ownProperties.msg.value, "Hello from the test add-on", "Should have got the right message");

    let consolePromise = addonDebugger.once("console");

    console.log("Bad message");
    Services.obs.notifyObservers(null, "addon-test-ping", "");

    let messageGrip = yield consolePromise;
    is(messageGrip.arguments[0].type, "object", "Should have logged an object");
    is(messageGrip.arguments[0].preview.ownProperties.msg.value, "Hello again", "Should have got the right message");

    yield addonDebugger.destroy();
    yield removeAddon(addon);
    finish();
  });
}
