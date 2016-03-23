/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the browser sharing document title listeners.
 */
"use strict";

var [, gHandlers] = LoopAPI.inspect();

function promiseBrowserSwitch() {
  return new Promise(resolve => {
    LoopAPI.stub([{
      sendAsyncMessage: function(messageName, data) {
        if (data[0] == "BrowserSwitch") {
          LoopAPI.restore();
          resolve();
        }
      }
    }]);
  });
}

add_task(function* setup() {
  Services.prefs.setBoolPref("loop.remote.autostart", true);

  gHandlers.AddBrowserSharingListener({ data: [42] }, () => {});

  let newTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", true);

  registerCleanupFunction(function* () {
    // Remove the listener.
    gHandlers.RemoveBrowserSharingListener({ data: [42] }, function() {});

    yield BrowserTestUtils.removeTab(newTab);

    Services.prefs.clearUserPref("loop.remote.autostart");
  });
});

add_task(function* test_notifyOnTitleChanged() {
  // Hook up the async listener and wait for the BrowserSwitch to happen.
  let browserSwitchPromise = promiseBrowserSwitch();

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:mozilla");

  // Now check we get the notification of the browser switch.
  yield browserSwitchPromise;

  Assert.ok(true, "We got notification of the browser switch");
});
