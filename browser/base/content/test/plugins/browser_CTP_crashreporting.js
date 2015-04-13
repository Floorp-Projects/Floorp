/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
const PLUGIN_PAGE = getRootDirectory(gTestPath) + "plugin_big.html";
const PLUGIN_SMALL_PAGE = getRootDirectory(gTestPath) + "plugin_small.html";

/**
 * Takes an nsIPropertyBag and converts it into a JavaScript Object. It
 * will also convert any nsIPropertyBag's within the nsIPropertyBag
 * recursively.
 *
 * @param aBag
 *        The nsIPropertyBag to convert.
 * @return Object
 *        Keyed on the names of the nsIProperty's within the nsIPropertyBag,
 *        and mapping to their values.
 */
function convertPropertyBag(aBag) {
  let result = {};
  let enumerator = aBag.enumerator;
  while(enumerator.hasMoreElements()) {
    let { name, value } = enumerator.getNext().QueryInterface(Ci.nsIProperty);
    if (value instanceof Ci.nsIPropertyBag) {
      value = convertPropertyBag(value);
    }
    result[name] = value;
  }
  return result;
}

function promisePopupNotificationShown(notificationID) {
  return new Promise((resolve) => {
    waitForNotificationShown(notificationID, resolve);
  });
}

add_task(function* setup() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  // The test harness sets MOZ_CRASHREPORTER_NO_REPORT, which disables plugin
  // crash reports.  This test needs them enabled.  The test also needs a mock
  // report server, and fortunately one is already set up by toolkit/
  // crashreporter/test/Makefile.in.  Assign its URL to MOZ_CRASHREPORTER_URL,
  // which CrashSubmit.jsm uses as a server override.
  let env = Cc["@mozilla.org/process/environment;1"].
            getService(Components.interfaces.nsIEnvironment);
  let noReport = env.get("MOZ_CRASHREPORTER_NO_REPORT");
  let serverURL = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_NO_REPORT", "");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);

  registerCleanupFunction(function cleanUp() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverURL);
  });
});

/**
 * Test that plugin crash submissions still work properly after
 * click-to-play activation.
 */
add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PLUGIN_PAGE,
  }, function* (browser) {
    let activated = yield ContentTask.spawn(browser, null, function*() {
      let plugin = content.document.getElementById("test");
      return plugin.QueryInterface(Ci.nsIObjectLoadingContent).activated;
    });
    ok(!activated, "Plugin should not be activated");

    // Open up the click-to-play notification popup...
    let popupNotification = PopupNotifications.getNotification("click-to-play-plugins",
                                                               browser);
    ok(popupNotification, "Should have a click-to-play notification");

    yield promisePopupNotificationShown(popupNotification);

    // The primary button in the popup should activate the plugin.
    PopupNotifications.panel.firstChild._primaryButton.click();

    // Prepare a crash report topic observer that only returns when
    // the crash report has been successfully sent.
    let crashReportChecker = (subject, data) => {
      return (data == "success");
    };
    let crashReportPromise = TestUtils.topicObserved("crash-report-status",
                                                     crashReportChecker);

    yield ContentTask.spawn(browser, null, function*() {
      let plugin = content.document.getElementById("test");
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);

      yield ContentTaskUtils.waitForCondition(() => {
        return plugin.activated;
      }, "Waited too long for plugin to activate.");

      try {
        plugin.crash();
      } catch(e) {}

      let doc = plugin.ownerDocument;

      let getUI = (anonid) => {
        return doc.getAnonymousElementByAttribute(plugin, "anonid", anonid);
      };

      // Now wait until the plugin crash report UI shows itself, which is
      // asynchronous.
      let statusDiv;

      yield ContentTaskUtils.waitForCondition(() => {
        statusDiv = getUI("submitStatus");
        return statusDiv.getAttribute("status") == "please";
      }, "Waited too long for plugin to show crash report UI");

      // Make sure the UI matches our expectations...
      let style = content.getComputedStyle(getUI("pleaseSubmit"));
      if (style.display != "block") {
        return Promise.reject(`Submission UI visibility is not correct. ` +
                              `Expected block style, got ${style.display}.`);
      }

      // Fill the crash report in with some test values that we'll test for in
      // the parent.
      getUI("submitComment").value = "a test comment";
      let optIn = getUI("submitURLOptIn");
      if (!optIn.checked) {
        return Promise.reject("URL opt-in should default to true.");
      }

      // Submit the report.
      optIn.click();
      getUI("submitButton").click();

      // And wait for the parent to say that the crash report was submitted
      // successfully.
      yield ContentTaskUtils.waitForCondition(() => {
        return statusDiv.getAttribute("status") == "success";
      }, "Timed out waiting for plugin binding to be in success state");
    });

    let [subject, data] = yield crashReportPromise;

    ok(subject instanceof Ci.nsIPropertyBag,
       "The crash report subject should be an nsIPropertyBag.");

    let crashData = convertPropertyBag(subject);
    ok(crashData.serverCrashID, "Should have a serverCrashID set.");

    // Remove the submitted report file after ensuring it exists.
    let file = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile);
    file.initWithPath(Services.crashmanager._submittedDumpsDir);
    file.append(crashData.serverCrashID + ".txt");
    ok(file.exists(), "Submitted report file should exist");
    file.remove(false);

    ok(crashData.extra, "Extra data should exist");
    is(crashData.extra.PluginUserComment, "a test comment",
       "Comment in extra data should match comment in textbox");

    is(crashData.extra.PluginContentURL, undefined,
       "URL should be absent from extra data when opt-in not checked");
  });
});

/**
 * Test that plugin crash submissions still work properly after
 * click-to-play with the notification bar.
 */
add_task(function*() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PLUGIN_SMALL_PAGE,
  }, function* (browser) {
    let activated = yield ContentTask.spawn(browser, null, function*() {
      let plugin = content.document.getElementById("test");
      return plugin.QueryInterface(Ci.nsIObjectLoadingContent).activated;
    });
    ok(!activated, "Plugin should not be activated");

    // Open up the click-to-play notification popup...
    let popupNotification = PopupNotifications.getNotification("click-to-play-plugins",
                                                               browser);
    ok(popupNotification, "Should have a click-to-play notification");

    yield promisePopupNotificationShown(popupNotification);

    // The primary button in the popup should activate the plugin.
    PopupNotifications.panel.firstChild._primaryButton.click();

    // Prepare a crash report topic observer that only returns when
    // the crash report has been successfully sent.
    let crashReportChecker = (subject, data) => {
      return (data == "success");
    };
    let crashReportPromise = TestUtils.topicObserved("crash-report-status",
                                                     crashReportChecker);

    yield ContentTask.spawn(browser, null, function*() {
      let plugin = content.document.getElementById("test");
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);

      yield ContentTaskUtils.waitForCondition(() => {
        return plugin.activated;
      }, "Waited too long for plugin to activate.");

      try {
        plugin.crash();
      } catch(e) {}
    });

    // Wait for the notification bar to be displayed.
    let notification = yield waitForNotificationBar("plugin-crashed", browser);

    // Then click the button to submit the crash report.
    let buttons = notification.querySelectorAll(".notification-button");
    is(buttons.length, 2, "Should have two buttons.");

    // The "Submit Crash Report" button should be the second one.
    let submitButton = buttons[1];
    submitButton.click();

    let [subject, data] = yield crashReportPromise;

    ok(subject instanceof Ci.nsIPropertyBag,
       "The crash report subject should be an nsIPropertyBag.");

    let crashData = convertPropertyBag(subject);
    ok(crashData.serverCrashID, "Should have a serverCrashID set.");

    // Remove the submitted report file after ensuring it exists.
    let file = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile);
    file.initWithPath(Services.crashmanager._submittedDumpsDir);
    file.append(crashData.serverCrashID + ".txt");
    ok(file.exists(), "Submitted report file should exist");
    file.remove(false);

    is(crashData.extra.PluginContentURL, undefined,
       "URL should be absent from extra data when opt-in not checked");
  });
});