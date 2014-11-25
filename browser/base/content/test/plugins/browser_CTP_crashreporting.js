/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
const gTestRoot = getRootDirectory(gTestPath);
var gTestBrowser = null;

/**
 * Frame script that will be injected into the test browser
 * to cause the crash, and then manipulate the crashed plugin
 * UI. Specifically, after the crash, we ensure that the
 * crashed plugin UI is using the right style rules and that
 * the submit URL opt-in defaults to checked. Then, we fill in
 * a comment with the crash report, uncheck the submit URL
 * opt-in, and send the crash reports.
 */
function frameScript() {
  function fail(reason) {
    sendAsyncMessage("test:crash-plugin:fail", {
      reason: `Failure from frameScript: ${reason}`,
    });
  }

  addMessageListener("test:crash-plugin", () => {
    let doc = content.document;

    addEventListener("PluginCrashed", (event) => {
      let plugin = doc.getElementById("test");
      if (!plugin) {
        fail("Could not find plugin element");
        return;
      }

      let getUI = (anonid) => {
        return doc.getAnonymousElementByAttribute(plugin, "anonid", anonid);
      };

      let style = content.getComputedStyle(getUI("pleaseSubmit"));
      if (style.display != "block") {
        fail("Submission UI visibility is not correct. Expected block, "
             + " got " + style.display);
        return;
      }

      getUI("submitComment").value = "a test comment";
      if (!getUI("submitURLOptIn").checked) {
        fail("URL opt-in should default to true.");
        return;
      }

      getUI("submitURLOptIn").click();
      getUI("submitButton").click();
    });

    let plugin = doc.getElementById("test");
    try {
      plugin.crash()
    } catch(e) {
    }
  });

  addMessageListener("test:plugin-submit-status", () => {
    let doc = content.document;
    let plugin = doc.getElementById("test");
    let submitStatusEl =
      doc.getAnonymousElementByAttribute(plugin, "anonid", "submitStatus");
    let submitStatus = submitStatusEl.getAttribute("status");
    sendAsyncMessage("test:plugin-submit-status:result", {
      submitStatus: submitStatus,
    });
  });
}

// Test that plugin crash submissions still work properly after
// click-to-play activation.

function test() {
  // Crashing the plugin takes up a lot of time, so extend the test timeout.
  requestLongerTimeout(2);
  waitForExplicitFinish();
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

  let tab = gBrowser.loadOneTab("about:blank", { inBackground: false });
  gTestBrowser = gBrowser.getBrowserForTab(tab);
  let mm = gTestBrowser.messageManager;
  mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", false);
  mm.addMessageListener("test:crash-plugin:fail", (message) => {
    ok(false, message.data.reason);
  });

  gTestBrowser.addEventListener("load", onPageLoad, true);
  Services.obs.addObserver(onSubmitStatus, "crash-report-status", false);

  registerCleanupFunction(function cleanUp() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverURL);
    gTestBrowser.removeEventListener("load", onPageLoad, true);
    Services.obs.removeObserver(onSubmitStatus, "crash-report-status");
    gBrowser.removeCurrentTab();
  });

  gTestBrowser.contentWindow.location = gTestRoot + "plugin_big.html";
}
function onPageLoad() {
  // Force the plugins binding to attach as layout is async.
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  plugin.clientTop;
  executeSoon(afterBindingAttached);
}

function afterBindingAttached() {
  let popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Should have a click-to-play notification");

  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Plugin should not be activated");

  // Simulate clicking the "Allow Always" button.
  waitForNotificationShown(popupNotification, function() {
    PopupNotifications.panel.firstChild._primaryButton.click();
    let condition = function() objLoadingContent.activated;
    waitForCondition(condition, pluginActivated, "Waited too long for plugin to activate");
  });
}

function pluginActivated() {
  let mm = gTestBrowser.messageManager;
  mm.sendAsyncMessage("test:crash-plugin");
}

function onSubmitStatus(subj, topic, data) {
  try {
    // Wait for success or failed, doesn't matter which.
    if (data != "success" && data != "failed")
      return;

    let propBag = subj.QueryInterface(Ci.nsIPropertyBag);
    if (data == "success") {
      let remoteID = getPropertyBagValue(propBag, "serverCrashID");
      ok(!!remoteID, "serverCrashID should be set");

      // Remove the submitted report file.
      let file = Cc["@mozilla.org/file/local;1"]
                   .createInstance(Ci.nsILocalFile);
      file.initWithPath(Services.crashmanager._submittedDumpsDir);
      file.append(remoteID + ".txt");
      ok(file.exists(), "Submitted report file should exist");
      file.remove(false);
    }

    let extra = getPropertyBagValue(propBag, "extra");
    ok(extra instanceof Ci.nsIPropertyBag, "Extra data should be property bag");

    let val = getPropertyBagValue(extra, "PluginUserComment");
    is(val, "a test comment",
       "Comment in extra data should match comment in textbox");

    val = getPropertyBagValue(extra, "PluginContentURL");
    ok(val === undefined,
       "URL should be absent from extra data when opt-in not checked");

    let submitStatus = null;
    let mm = gTestBrowser.messageManager;
    mm.addMessageListener("test:plugin-submit-status:result", (message) => {
      submitStatus = message.data.submitStatus;
    });

    mm.sendAsyncMessage("test:plugin-submit-status");

    let condition = () => submitStatus;
    waitForCondition(condition, () => {
      is(submitStatus, data, "submitStatus data should match");
      finish();
    }, "Waiting for submitStatus to be reported from frame script");
  }
  catch (err) {
    failWithException(err);
  }
}

function getPropertyBagValue(bag, key) {
  try {
    var val = bag.getProperty(key);
  }
  catch (e if e.result == Cr.NS_ERROR_FAILURE) {}
  return val;
}

function failWithException(err) {
  ok(false, "Uncaught exception: " + err + "\n" + err.stack);
}
