"use strict";

const PAGE =
  "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const EMAIL = "foo@privacy.com";

add_task(async function setup() {
  await setupLocalCrashReportServer();
  // By default, requesting the email address of the user is disabled.
  // For the purposes of this test, we turn it back on.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.crashReporting.requestEmail", true]],
  });
});

/**
 * Test that if we have an email address stored in prefs, and we decide
 * not to submit the email address in the next crash report, that we
 * clear the email address.
 */
add_task(async function test_clear_email() {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async function(browser) {
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
      await BrowserTestUtils.crashFrame(
        browser,
        /* shouldShowTabCrashPage */ true,
        /* shouldClearMinidumps */ false
      );
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
      await BrowserTestUtils.waitForEvent(tab, "SSTabRestored");
      await crashReport;

      is(prefs.getCharPref("email"), "", "No email address should be stored");

      // Submitting the crash report may have set some prefs regarding how to
      // send tab crash reports. Let's reset them for the next test.
      prefs.setBoolPref("sendReport", originalSendReport);
      prefs.setBoolPref("emailMe", originalEmailMe);
      prefs.setBoolPref("includeURL", originalIncludeURL);
      prefs.setCharPref("email", originalEmail);
    }
  );
});
