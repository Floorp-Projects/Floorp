Cu.import("resource://gre/modules/CrashSubmit.jsm", this);

const CRASH_URL = "http://example.com/browser/browser/base/content/test/plugins/plugin_crashCommentAndURL.html";
const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";

/**
 * Frame script that will be injected into the test browser
 * to cause plugin crashes, and then manipulate the crashed plugin
 * UI. The specific actions and checks that occur in the frame
 * script for the crashed plugin UI are set in the
 * test:crash-plugin message object sent from the parent. The actions
 * and checks that the parent can specify are:
 *
 * pleaseSubmitStyle: the display style that the pleaseSubmit anonymous element
 *                    should have - example "block", "none".
 * submitComment:     the comment that should be put into the crash report
 * urlOptIn:          true if the submitURLOptIn element should be checked.
 * sendCrashMessage:  if true, the frame script will send a
 *                    test:crash-plugin:crashed message when the plugin has
 *                    crashed. This is used for the last test case, and
 *                    causes the frame script to skip any of the crashed
 *                    plugin UI manipulation, since the last test shows
 *                    no crashed plugin UI.
 */
function frameScript() {
  function fail(reason) {
    sendAsyncMessage("test:crash-plugin:fail", {
      reason: `Failure from frameScript: ${reason}`,
    });
  }

  addMessageListener("test:crash-plugin", (message) => {
    addEventListener("PluginCrashed", function onPluginCrashed(event) {
      removeEventListener("PluginCrashed", onPluginCrashed);

      let doc = content.document;
      let plugin = doc.getElementById("plugin");
      if (!plugin) {
        fail("Could not find plugin element");
        return;
      }

      let getUI = (anonid) => {
        return doc.getAnonymousElementByAttribute(plugin, "anonid", anonid);
      };

      let style = content.getComputedStyle(getUI("pleaseSubmit"));
      if (style.display != message.data.pleaseSubmitStyle) {
        fail("Submission UI visibility is not correct. Expected " +
             `${message.data.pleaseSubmitStyle} and got ${style.display}`);
        return;
      }

      if (message.data.sendCrashMessage) {
        sendAsyncMessage("test:crash-plugin:crashed", {
          crashID: event.pluginDumpID,
        });
        return;
      }

      if (message.data.submitComment) {
        getUI("submitComment").value = message.data.submitComment;
      }
      getUI("submitURLOptIn").checked = message.data.urlOptIn;
      getUI("submitButton").click();
    });

    let plugin = content.document.getElementById("test");
    try {
      plugin.crash()
    } catch(e) {
    }
  });
}

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
  let mm = browser.messageManager;
  mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", true);

  mm.addMessageListener("test:crash-plugin:fail", (message) => {
    ok(false, message.data.reason);
  });

  Services.obs.addObserver(onSubmitStatus, "crash-report-status", false);

  registerCleanupFunction(function cleanUp() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverURL);
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
    let mm = gBrowser.selectedBrowser.messageManager;

    if (!currentRun.shouldSubmittionUIBeVisible) {
      mm.addMessageListener("test:crash-plugin:crash", function onCrash(message) {
        mm.removeMessageListener("test:crash-plugin:crash", onCrash);

        ok(!!message.data.crashID, "pluginDumpID should be set");
        CrashSubmit.delete(message.data.crashID);
        doNextRun();
      });
    }

    mm.sendAsyncMessage("test:crash-plugin", {
      pleaseSubmitStyle: currentRun.shouldSubmissionUIBeVisible ? "block" : "none",
      submitComment: currentRun.comment,
      urlOptIn: currentRun.urlOptIn,
      sendOnCrashMessage: !currentRun.shouldSubmissionUIBeVisible,
    });
    gBrowser.loadURI(CRASH_URL + "?" +
                     encodeURIComponent(JSON.stringify(args)));
    // And now wait for the crash.
  }
  catch (err) {
    failWithException(err);
    finish();
  }
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
