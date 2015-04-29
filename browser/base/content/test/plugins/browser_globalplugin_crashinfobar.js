/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that plugin crash submissions still work properly after
 * click-to-play activation.
 */
add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:blank",
  }, function* (browser) {
    yield ContentTask.spawn(browser, null, function* () {
      const GMP_CRASH_EVENT = {
        pluginName: "GlobalTestPlugin",
        pluginDumpID: "1234",
        browserDumpID: "5678",
        submittedCrashReport: false,
        bubbles: true,
        cancelable: true,
        gmpPlugin: true,
      };

      let crashEvent = new content.PluginCrashedEvent("PluginCrashed",
                                                      GMP_CRASH_EVENT);
      content.dispatchEvent(crashEvent);
    });

    let notification = yield waitForNotificationBar("plugin-crashed", browser);

    let notificationBox = gBrowser.getNotificationBox(browser);
    ok(notification, "Infobar was shown.");
    is(notification.priority, notificationBox.PRIORITY_WARNING_MEDIUM,
       "Correct priority.");
    is(notification.getAttribute("label"),
       "The GlobalTestPlugin plugin has crashed.",
       "Correct message.");
  });
});
