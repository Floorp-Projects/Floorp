"use strict";

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const EMAIL = "foo@privacy.com";

/**
 * Sets up the browser to send crash reports to the local crash report
 * testing server.
 */
add_task(function* setup() {
  // The test harness sets MOZ_CRASHREPORTER_NO_REPORT, which disables crash
  // reports.  This test needs them enabled.  The test also needs a mock
  // report server, and fortunately one is already set up by toolkit/
  // crashreporter/test/Makefile.in.  Assign its URL to MOZ_CRASHREPORTER_URL,
  // which CrashSubmit.jsm uses as a server override.
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Components.interfaces.nsIEnvironment);
  let noReport = env.get("MOZ_CRASHREPORTER_NO_REPORT");
  let serverUrl = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_NO_REPORT", "");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);

  registerCleanupFunction(function() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverUrl);
  });
});

/**
 * Test that if we have an email address stored in prefs, and we decide
 * not to submit the email address in the next crash report, that we
 * clear the email address.
 */
add_task(function* test_clear_email() {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
    let prefs = TabCrashHandler.prefs;
    let originalSendReport = prefs.getBoolPref("sendReport");
    let originalEmailMe = prefs.getBoolPref("emailMe");
    let originalIncludeURL = prefs.getBoolPref("includeURL");
    let originalEmail = prefs.getCharPref("email");

    // Pretend that we stored an email address from the previous
    // crash
    prefs.setCharPref("email", EMAIL);
    prefs.setBoolPref("emailMe", true);

    let tab = gBrowser.getTabForBrowser(browser);
    yield BrowserTestUtils.crashBrowser(browser);
    let doc = browser.contentDocument;

    // Since about:tabcrashed will run in the parent process, we can safely
    // manipulate its DOM nodes directly
    let emailMe = doc.getElementById("emailMe");
    emailMe.checked = false;

    let crashReport = promiseCrashReport({
      Email: "",
    });

    let restoreTab = browser.contentDocument.getElementById("restoreTab");
    restoreTab.click();
    yield BrowserTestUtils.waitForEvent(tab, "SSTabRestored");
    yield crashReport;

    is(prefs.getCharPref("email"), "", "No email address should be stored");

    // Submitting the crash report may have set some prefs regarding how to
    // send tab crash reports. Let's reset them for the next test.
    prefs.setBoolPref("sendReport", originalSendReport);
    prefs.setBoolPref("emailMe", originalEmailMe);
    prefs.setBoolPref("includeURL", originalIncludeURL);
    prefs.setCharPref("email", originalEmail);
  });
});

