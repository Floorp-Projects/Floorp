"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

/**
 * Returns a Promise that resolves once a crash report has
 * been submitted. This function will also test the crash
 * reports extra data to see if it matches expectedExtra.
 *
 * @param expectedExtra (object)
 *        An Object whose key-value pairs will be compared
 *        against the key-value pairs in the extra data of the
 *        crash report. A test failure will occur if there is
 *        a mismatch.
 *
 *        If the value of the key-value pair is "null", this will
 *        be interpreted as "this key should not be included in the
 *        extra data", and will cause a test failure if it is detected
 *        in the crash report.
 *
 *        Note that this will ignore any keys that are not included
 *        in expectedExtra. It's possible that the crash report
 *        will contain other extra information that is not
 *        compared against.
 * @returns Promise
 */
function promiseCrashReport(expectedExtra = {}) {
  return (async function() {
    info("Starting wait on crash-report-status");
    let [subject] = await TestUtils.topicObserved(
      "crash-report-status",
      (unused, data) => {
        return data == "success";
      }
    );
    info("Topic observed!");

    if (!(subject instanceof Ci.nsIPropertyBag2)) {
      throw new Error("Subject was not a Ci.nsIPropertyBag2");
    }

    let remoteID = getPropertyBagValue(subject, "serverCrashID");
    if (!remoteID) {
      throw new Error("Report should have a server ID");
    }

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(Services.crashmanager._submittedDumpsDir);
    file.append(remoteID + ".txt");
    if (!file.exists()) {
      throw new Error("Report should have been received by the server");
    }

    file.remove(false);

    let extra = getPropertyBagValue(subject, "extra");
    if (!(extra instanceof Ci.nsIPropertyBag2)) {
      throw new Error("extra was not a Ci.nsIPropertyBag2");
    }

    info("Iterating crash report extra keys");
    for (let { name: key } of extra.enumerator) {
      let value = extra.getPropertyAsAString(key);
      if (key in expectedExtra) {
        if (expectedExtra[key] == null) {
          ok(false, `Got unexpected key ${key} with value ${value}`);
        } else {
          is(
            value,
            expectedExtra[key],
            `Crash report had the right extra value for ${key}`
          );
        }
      }
    }
  })();
}

/**
 * For an nsIPropertyBag, returns the value for a given
 * key.
 *
 * @param bag
 *        The nsIPropertyBag to retrieve the value from
 * @param key
 *        The key that we want to get the value for from the
 *        bag
 * @returns The value corresponding to the key from the bag,
 *          or null if the value could not be retrieved (for
 *          example, if no value is set at that key).
 */
function getPropertyBagValue(bag, key) {
  try {
    let val = bag.getProperty(key);
    return val;
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  return null;
}

/**
 * Sets up the browser to send crash reports to the local crash report
 * testing server.
 */
async function setupLocalCrashReportServer() {
  const SERVER_URL =
    "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";

  // The test harness sets MOZ_CRASHREPORTER_NO_REPORT, which disables crash
  // reports.  This test needs them enabled.  The test also needs a mock
  // report server, and fortunately one is already set up by toolkit/
  // crashreporter/test/Makefile.in.  Assign its URL to MOZ_CRASHREPORTER_URL,
  // which CrashSubmit.jsm uses as a server override.
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let noReport = env.get("MOZ_CRASHREPORTER_NO_REPORT");
  let serverUrl = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_NO_REPORT", "");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);

  registerCleanupFunction(function() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverUrl);
  });
}

/**
 * Monkey patches TabCrashHandler.getDumpID to return null in order to test
 * about:tabcrashed when a dump is not available.
 */
function prepareNoDump() {
  let originalGetDumpID = TabCrashHandler.getDumpID;
  TabCrashHandler.getDumpID = function(browser) {
    return null;
  };
  registerCleanupFunction(() => {
    TabCrashHandler.getDumpID = originalGetDumpID;
  });
}

const kBuildidMatchEnv = "MOZ_BUILDID_MATCH_DONTSEND";

function setBuildidMatchDontSendEnv() {
  const envService = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  info("Setting " + kBuildidMatchEnv + "=1");
  envService.set(kBuildidMatchEnv, "1");
  info("Set " + kBuildidMatchEnv + "=1");
}

function unsetBuildidMatchDontSendEnv() {
  const envService = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  info("Setting " + kBuildidMatchEnv + "=0");
  envService.set(kBuildidMatchEnv, "0");
  info("Set " + kBuildidMatchEnv + "=0");
}

function getEventPromise(eventName, eventKind, kTimeout) {
  return new Promise(function(resolve, reject) {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    let maybeTimeout = setTimeout(() => {
      ok(
        false,
        "Timed out waiting " + eventName + " (" + eventKind + ") event"
      );
      reject();
    }, kTimeout);

    info("Installing event listener (" + eventKind + ")");
    window.addEventListener(
      eventName,
      event => {
        info("Clear timeout for " + eventKind + " event");
        clearTimeout(maybeTimeout);
        ok(true, "Received " + eventName + " (" + eventKind + ") event");
        info("Call resolve() for " + eventKind + " event");
        resolve();
      },
      { once: true }
    );
    info("Installed event listener (" + eventKind + ")");
  });
}

async function openNewTab(forceCrash) {
  const PAGE =
    "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

  let options = {
    gBrowser,
    PAGE,
    waitForLoad: false,
    waitForStateStop: false,
    forceNewProcess: true,
  };

  let tab = await BrowserTestUtils.openNewForegroundTab(options);

  if (forceCrash === true) {
    let browser = tab.linkedBrowser;
    await BrowserTestUtils.crashFrame(browser, true, true, null);
  }

  // Since we crashed early, we expect to have some about:blank
  // Remove it to clean up
  BrowserTestUtils.removeTab(tab);
}

function getFalsePositiveTelemetry() {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  return scalars["dom.contentprocess.buildID_mismatch_false_positive"];
}
