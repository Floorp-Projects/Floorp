/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

const CRASH_URL = "http://example.com/browser/browser/base/content/test/plugins/plugin_crashCommentAndURL.html";

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";

function test() {
  // Crashing the plugin takes up a lot of time, so extend the test timeout.
  requestLongerTimeout(runs.length);
  waitForExplicitFinish();
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

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
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("PluginCrashed", onCrash, false);
  Services.obs.addObserver(onSubmitStatus, "crash-report-status", false);

  registerCleanupFunction(function cleanUp() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverURL);
    gBrowser.selectedBrowser.removeEventListener("PluginCrashed", onCrash,
                                                 false);
    Services.obs.removeObserver(onSubmitStatus, "crash-report-status");
    gBrowser.removeCurrentTab();
  });

  doNextRun();
}

let runs = [
  {
    shouldSubmissionUIBeVisible: true,
    comment: "",
    urlOptIn: false,
  },
  {
    shouldSubmissionUIBeVisible: true,
    comment: "a test comment",
    urlOptIn: true,
  },
  {
    width: 300,
    height: 300,
    shouldSubmissionUIBeVisible: false,
  },
];

let currentRun = null;

function doNextRun() {
  try {
    if (!runs.length) {
      finish();
      return;
    }
    currentRun = runs.shift();
    let args = ["width", "height"].reduce(function (memo, arg) {
      if (arg in currentRun)
        memo[arg] = currentRun[arg];
      return memo;
    }, {});
    gBrowser.loadURI(CRASH_URL + "?" +
                     encodeURIComponent(JSON.stringify(args)));
    // And now wait for the crash.
  }
  catch (err) {
    failWithException(err);
    finish();
  }
}

function onCrash() {
  try {
    let plugin = gBrowser.contentDocument.getElementById("plugin");
    let elt = gPluginHandler.getPluginUI.bind(gPluginHandler, plugin);
    let style =
      gBrowser.contentWindow.getComputedStyle(elt("pleaseSubmit"));
    is(style.display,
       currentRun.shouldSubmissionUIBeVisible ? "block" : "none",
       "Submission UI visibility should be correct");
    if (!currentRun.shouldSubmissionUIBeVisible) {
      // Done with this run.
      doNextRun();
      return;
    }
    elt("submitComment").value = currentRun.comment;
    elt("submitURLOptIn").checked = currentRun.urlOptIn;
    elt("submitButton").click();
    // And now wait for the submission status notification.
  }
  catch (err) {
    failWithException(err);
    doNextRun();
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
    if (currentRun.comment)
      is(val, currentRun.comment,
         "Comment in extra data should match comment in textbox");
    else
      ok(val === undefined,
         "Comment should be absent from extra data when textbox is empty");

    val = getPropertyBagValue(extra, "PluginContentURL");
    if (currentRun.urlOptIn)
      is(val, gBrowser.currentURI.spec,
         "URL in extra data should match browser URL when opt-in checked");
    else
      ok(val === undefined,
         "URL should be absent from extra data when opt-in not checked");
  }
  catch (err) {
    failWithException(err);
  }
  doNextRun();
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
