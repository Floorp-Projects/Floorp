Cu.import("resource://gre/modules/CrashSubmit.jsm", this);

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";

var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gTestBrowser = null;
var config = {};

add_task(async function() {
  // The test harness sets MOZ_CRASHREPORTER_NO_REPORT, which disables plugin
  // crash reports.  This test needs them enabled.  The test also needs a mock
  // report server, and fortunately one is already set up by toolkit/
  // crashreporter/test/Makefile.in.  Assign its URL to MOZ_CRASHREPORTER_URL,
  // which CrashSubmit.jsm uses as a server override.
  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  let noReport = env.get("MOZ_CRASHREPORTER_NO_REPORT");
  let serverUrl = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_NO_REPORT", "");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  // Crash immediately
  Services.prefs.setIntPref("dom.ipc.plugins.timeoutSecs", 0);

  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("dom.ipc.plugins.timeoutSecs");
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverUrl);
    env = null;
    config = null;
    gTestBrowser = null;
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

add_task(async function() {
  config = {
    shouldSubmissionUIBeVisible: true,
    comment: "",
    urlOptIn: false
  };

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  let pluginCrashed = promisePluginCrashed();

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_crashCommentAndURL.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // Wait for the plugin to crash
  await pluginCrashed;

  let crashReportStatus = TestUtils.topicObserved("crash-report-status", onSubmitStatus);

  // Test that the crash submission UI is actually visible and submit the crash report.
  await ContentTask.spawn(gTestBrowser, config, async function(aConfig) {
    let doc = content.document;
    let plugin = doc.getElementById("plugin");
    let pleaseSubmit = doc.getAnonymousElementByAttribute(plugin, "anonid", "pleaseSubmit");
    let submitButton = doc.getAnonymousElementByAttribute(plugin, "anonid", "submitButton");
    // Test that we don't send the URL when urlOptIn is false.
    doc.getAnonymousElementByAttribute(plugin, "anonid", "submitURLOptIn").checked = aConfig.urlOptIn;
    submitButton.click();
    Assert.equal(content.getComputedStyle(pleaseSubmit).display == "block",
      aConfig.shouldSubmissionUIBeVisible, "The crash UI should be visible");
  });

  await crashReportStatus;
});

add_task(async function() {
  config = {
    shouldSubmissionUIBeVisible: true,
    comment: "a test comment",
    urlOptIn: true
  };

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  let pluginCrashed = promisePluginCrashed();

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_crashCommentAndURL.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // Wait for the plugin to crash
  await pluginCrashed;

  let crashReportStatus = TestUtils.topicObserved("crash-report-status", onSubmitStatus);

  // Test that the crash submission UI is actually visible and submit the crash report.
  await ContentTask.spawn(gTestBrowser, config, async function(aConfig) {
    let doc = content.document;
    let plugin = doc.getElementById("plugin");
    let pleaseSubmit = doc.getAnonymousElementByAttribute(plugin, "anonid", "pleaseSubmit");
    let submitButton = doc.getAnonymousElementByAttribute(plugin, "anonid", "submitButton");
    // Test that we send the URL when urlOptIn is true.
    doc.getAnonymousElementByAttribute(plugin, "anonid", "submitURLOptIn").checked = aConfig.urlOptIn;
    doc.getAnonymousElementByAttribute(plugin, "anonid", "submitComment").value = aConfig.comment;
    submitButton.click();
    Assert.equal(content.getComputedStyle(pleaseSubmit).display == "block",
      aConfig.shouldSubmissionUIBeVisible, "The crash UI should be visible");
  });

  await crashReportStatus;
});

add_task(async function() {
  config = {
    shouldSubmissionUIBeVisible: false,
    comment: "",
    urlOptIn: true
  };

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  let pluginCrashed = promisePluginCrashed();

  // Make sure that the plugin container is too small to display the crash submission UI
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_crashCommentAndURL.html?" +
                            encodeURIComponent(JSON.stringify({width: 300, height: 300})));

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  // Wait for the plugin to crash
  await pluginCrashed;

  // Test that the crash submission UI is not visible and do not submit a crash report.
  await ContentTask.spawn(gTestBrowser, config, async function(aConfig) {
    let doc = content.document;
    let plugin = doc.getElementById("plugin");
    let pleaseSubmit = doc.getAnonymousElementByAttribute(plugin, "anonid", "pleaseSubmit");
    Assert.equal(!!pleaseSubmit && content.getComputedStyle(pleaseSubmit).display == "block",
      aConfig.shouldSubmissionUIBeVisible, "Plugin crash UI should not be visible");
  });
});

function promisePluginCrashed() {
  return new ContentTask.spawn(gTestBrowser, {}, async function() {
    await new Promise((resolve) => {
      addEventListener("PluginCrashReporterDisplayed", function onPluginCrashed() {
        removeEventListener("PluginCrashReporterDisplayed", onPluginCrashed);
        resolve();
      });
    });
  });
}

function onSubmitStatus(aSubject, aData) {
  // Wait for success or failed, doesn't matter which.
  if (aData != "success" && aData != "failed")
    return false;

  let propBag = aSubject.QueryInterface(Ci.nsIPropertyBag);
  if (aData == "success") {
    let remoteID = getPropertyBagValue(propBag, "serverCrashID");
    ok(!!remoteID, "serverCrashID should be set");

    // Remove the submitted report file.
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(Services.crashmanager._submittedDumpsDir);
    file.append(remoteID + ".txt");
    ok(file.exists(), "Submitted report file should exist");
    file.remove(false);
  }

  let extra = getPropertyBagValue(propBag, "extra");
  ok(extra instanceof Ci.nsIPropertyBag, "Extra data should be property bag");

  let val = getPropertyBagValue(extra, "PluginUserComment");
  if (config.comment)
    is(val, config.comment,
       "Comment in extra data should match comment in textbox");
  else
    ok(val === undefined,
       "Comment should be absent from extra data when textbox is empty");

  val = getPropertyBagValue(extra, "PluginContentURL");
  if (config.urlOptIn)
    is(val, gBrowser.currentURI.spec,
       "URL in extra data should match browser URL when opt-in checked");
  else
    ok(val === undefined,
       "URL should be absent from extra data when opt-in not checked");

  return true;
}

function getPropertyBagValue(bag, key) {
  try {
    var val = bag.getProperty(key);
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }
  return val;
}
