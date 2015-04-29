/* This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gTestRoot = getRootDirectory(gTestPath);
const gHttpTestRoot = gTestRoot.replace("chrome://mochitests/content/",
                                        "http://127.0.0.1:8888/");

add_task(function* () {
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("plugins.click_to_play");
  });
})

/**
 * Tests that if a plugin is removed just as we transition to
 * a different page, that we don't show the hidden plugin
 * notification bar on the new page.
 */
add_task(function* () {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  // Load up a page with a plugin...
  let notificationPromise =
    waitForNotificationBar("plugin-hidden", gBrowser.selectedBrowser);
  yield loadPage(browser, gHttpTestRoot + "plugin_small.html")
  yield forcePluginBindingAttached(browser);
  yield notificationPromise;

  // Trigger the PluginRemoved event to be fired, and then immediately
  // browse to a new page.
  let plugin = browser.contentDocument.getElementById("test");
  plugin.remove();
  yield loadPage(browser, "about:mozilla");

  // There should be no hidden plugin notification bar at about:mozilla.
  let notificationBox = gBrowser.getNotificationBox(browser);
  is(notificationBox.getNotificationWithValue("plugin-hidden"), null,
     "Expected no notification box");
  gBrowser.removeTab(newTab);
});

/**
 * Tests that if a plugin is removed just as we transition to
 * a different page with a plugin, that we show the right notification
 * for the new page.
 */
add_task(function* () {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY,
                            "Second Test Plug-in");

  // Load up a page with a plugin...
  let notificationPromise =
    waitForNotificationBar("plugin-hidden", browser);
  yield loadPage(browser, gHttpTestRoot + "plugin_small.html")
  yield forcePluginBindingAttached(browser);
  yield notificationPromise;

  // Trigger the PluginRemoved event to be fired, and then immediately
  // browse to a new page.
  let plugin = browser.contentDocument.getElementById("test");
  plugin.remove();
  yield loadPage(browser, gTestRoot + "plugin_small_2.html");
  let notification = yield waitForNotificationBar("plugin-hidden", browser);
  ok(notification, "There should be a notification shown for the new page.");

  // Ensure that the notification is showing information about
  // the x-second-test plugin.
  ok(notification.label.includes("Second Test"), "Should mention the second plugin");
  ok(!notification.label.includes("127.0.0.1"), "Should not refer to old principal");
  ok(notification.label.includes("null"), "Should refer to the new principal");
  gBrowser.removeTab(newTab);
});
