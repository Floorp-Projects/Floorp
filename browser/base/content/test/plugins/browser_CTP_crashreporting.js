/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
const gTestRoot = getRootDirectory(gTestPath);
var gTestBrowser = null;

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
  gTestBrowser.addEventListener("PluginCrashed", onCrash, false);
  gTestBrowser.addEventListener("load", onPageLoad, true);
  Services.obs.addObserver(onSubmitStatus, "crash-report-status", false);

  registerCleanupFunction(function cleanUp() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverURL);
    gTestBrowser.removeEventListener("PluginCrashed", onCrash, false);
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
  popupNotification.reshow();
  PopupNotifications.panel.firstChild._primaryButton.click();

  let condition = function() objLoadingContent.activated;
  waitForCondition(condition, pluginActivated, "Waited too long for plugin to activate");
}

function pluginActivated() {
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  try {
    plugin.crash();
  } catch (e) {
    // The plugin crashed in the above call, an exception is expected.
  }
}

function onCrash() {
  try {
    let plugin = gBrowser.contentDocument.getElementById("test");
    let elt = gPluginHandler.getPluginUI.bind(gPluginHandler, plugin);
    let style =
      gBrowser.contentWindow.getComputedStyle(elt("pleaseSubmit"));
    is(style.display, "block", "Submission UI visibility should be correct");

    elt("submitComment").value = "a test comment";
    is(elt("submitURLOptIn").checked, true, "URL opt-in should default to true");
    EventUtils.synthesizeMouseAtCenter(elt("submitURLOptIn"), {}, gTestBrowser.contentWindow);
    EventUtils.synthesizeMouseAtCenter(elt("submitButton"), {}, gTestBrowser.contentWindow);
    // And now wait for the submission status notification.
  }
  catch (err) {
    failWithException(err);
  }
}

function onSubmitStatus(subj, topic, data) {
  try {
    // Wait for success or failed, doesn't matter which.
    if (data != "success" && data != "failed")
      return;

    let extra = getPropertyBagValue(subj.QueryInterface(Ci.nsIPropertyBag),
                                    "extra");
    ok(extra instanceof Ci.nsIPropertyBag, "Extra data should be property bag");

    let val = getPropertyBagValue(extra, "PluginUserComment");
    is(val, "a test comment",
       "Comment in extra data should match comment in textbox");

    val = getPropertyBagValue(extra, "PluginContentURL");
    ok(val === undefined,
       "URL should be absent from extra data when opt-in not checked");

    // Execute this later in case the event to change submitStatus has not
    // have been dispatched yet.
    executeSoon(function () {
      let plugin = gBrowser.contentDocument.getElementById("test");
      let elt = gPluginHandler.getPluginUI.bind(gPluginHandler, plugin);
      is(elt("submitStatus").getAttribute("status"), data,
         "submitStatus data should match");
    });
  }
  catch (err) {
    failWithException(err);
  }
  finish();
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
