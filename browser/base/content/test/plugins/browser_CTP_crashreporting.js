var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
const PLUGIN_PAGE = gTestRoot + "plugin_big.html";
const PLUGIN_SMALL_PAGE = gTestRoot + "plugin_small.html";

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
  while (enumerator.hasMoreElements()) {
    let { name, value } = enumerator.getNext().QueryInterface(Ci.nsIProperty);
    if (value instanceof Ci.nsIPropertyBag) {
      value = convertPropertyBag(value);
    }
    result[name] = value;
  }
  return result;
}

add_task(async function setup() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Test Plug-in");

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

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  registerCleanupFunction(function cleanUp() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverURL);
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    window.focus();
  });
});

/**
 * Test that plugin crash submissions still work properly after
 * click-to-play activation.
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PLUGIN_PAGE,
  }, async function(browser) {
    // Work around for delayed PluginBindingAttached
    await promiseUpdatePluginBindings(browser);

    let pluginInfo = await promiseForPluginInfo("test", browser);
    ok(!pluginInfo.activated, "Plugin should not be activated");

    // Simulate clicking the "Allow Always" button.
    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);
    await promiseForNotificationShown(notification, browser);
    PopupNotifications.panel.firstChild._primaryButton.click();

    // Prepare a crash report topic observer that only returns when
    // the crash report has been successfully sent.
    let crashReportChecker = (subject, data) => {
      return (data == "success");
    };
    let crashReportPromise = TestUtils.topicObserved("crash-report-status",
                                                     crashReportChecker);

    await ContentTask.spawn(browser, null, async function() {
      let plugin = content.document.getElementById("test");
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);

      await ContentTaskUtils.waitForCondition(() => {
        return plugin.activated;
      }, "Waited too long for plugin to activate.");

      try {
        Components.utils.waiveXrays(plugin).crash();
      } catch (e) {
      }

      let doc = plugin.ownerDocument;

      let getUI = (anonid) => {
        return doc.getAnonymousElementByAttribute(plugin, "anonid", anonid);
      };

      // Now wait until the plugin crash report UI shows itself, which is
      // asynchronous.
      let statusDiv;

      await ContentTaskUtils.waitForCondition(() => {
        statusDiv = getUI("submitStatus");
        return statusDiv.getAttribute("status") == "please";
      }, "Waited too long for plugin to show crash report UI");

      // Make sure the UI matches our expectations...
      let style = content.getComputedStyle(getUI("pleaseSubmit"));
      if (style.display != "block") {
        throw new Error(`Submission UI visibility is not correct. ` +
                        `Expected block style, got ${style.display}.`);
      }

      // Fill the crash report in with some test values that we'll test for in
      // the parent.
      getUI("submitComment").value = "a test comment";
      let optIn = getUI("submitURLOptIn");
      if (!optIn.checked) {
        throw new Error("URL opt-in should default to true.");
      }

      // Submit the report.
      optIn.click();
      getUI("submitButton").click();

      // And wait for the parent to say that the crash report was submitted
      // successfully. This can take time on debug builds.
      await ContentTaskUtils.waitForCondition(() => {
        return statusDiv.getAttribute("status") == "success";
      }, "Timed out waiting for plugin binding to be in success state",
      100, 200);
    });

    let [subject, ] = await crashReportPromise;

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
add_task(async function() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PLUGIN_SMALL_PAGE,
  }, async function(browser) {
    // Work around for delayed PluginBindingAttached
    await promiseUpdatePluginBindings(browser);

    let pluginInfo = await promiseForPluginInfo("test", browser);
    ok(pluginInfo.activated, "Plugin should be activated from previous test");

    // Prepare a crash report topic observer that only returns when
    // the crash report has been successfully sent.
    let crashReportChecker = (subject, data) => {
      return (data == "success");
    };
    let crashReportPromise = TestUtils.topicObserved("crash-report-status",
                                                     crashReportChecker);

    await ContentTask.spawn(browser, null, async function() {
      let plugin = content.document.getElementById("test");
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);

      await ContentTaskUtils.waitForCondition(() => {
        return plugin.activated;
      }, "Waited too long for plugin to activate.");

      try {
        Components.utils.waiveXrays(plugin).crash();
      } catch (e) {}
    });

    // Wait for the notification bar to be displayed.
    let notification = await waitForNotificationBar("plugin-crashed", browser);

    // Then click the button to submit the crash report.
    let buttons = notification.querySelectorAll(".notification-button");
    is(buttons.length, 2, "Should have two buttons.");

    // The "Submit Crash Report" button should be the second one.
    let submitButton = buttons[1];
    submitButton.click();

    let [subject, ] = await crashReportPromise;

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
