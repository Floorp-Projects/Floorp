/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTestBrowser = null;

let crashedEventProperties = {
  pluginName: "GlobalTestPlugin",
  pluginDumpID: "1234",
  browserDumpID: "5678",
  submittedCrashReport: false,
  bubbles: true,
  cancelable: true
}

// Test that plugin crash submissions still work properly after
// click-to-play activation.

function test() {
  waitForExplicitFinish();
  let tab = gBrowser.loadOneTab("about:blank", { inBackground: false });
  gTestBrowser = gBrowser.getBrowserForTab(tab);
  gTestBrowser.addEventListener("PluginCrashed", onCrash, false);
  gTestBrowser.addEventListener("load", onPageLoad, true);

  registerCleanupFunction(function cleanUp() {
    gTestBrowser.removeEventListener("PluginCrashed", onCrash, false);
    gTestBrowser.removeEventListener("load", onPageLoad, true);
    gBrowser.removeTab(tab);
  });
}

function onPageLoad() {
  executeSoon(generateCrashEvent);
}

function generateCrashEvent() {
  let window = gTestBrowser.contentWindow;
  let crashedEvent = new window.PluginCrashedEvent("PluginCrashed", crashedEventProperties);

  window.dispatchEvent(crashedEvent);
}


function onCrash(event) {
  let target = event.target;
  is (target, gTestBrowser.contentWindow, "Event target is the window.");

  for (let [name, val] of Iterator(crashedEventProperties)) {
    let propVal = event[name];

    is (propVal, val, "Correct property: " + name + ".");
  }

  waitForNotificationBar("plugin-crashed", gTestBrowser, (notification) => {
    let notificationBox = gBrowser.getNotificationBox(gTestBrowser);
    ok(notification, "Infobar was shown.");
    is(notification.priority, notificationBox.PRIORITY_WARNING_MEDIUM, "Correct priority.");
    is(notification.getAttribute("label"), "The GlobalTestPlugin plugin has crashed.", "Correct message.");
    finish();
  });
}
